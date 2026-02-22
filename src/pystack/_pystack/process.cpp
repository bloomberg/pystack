#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <memory>

#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <utility>
#include <vector>

#include "corefile.h"
#include "logging.h"
#include "mem.h"
#include "native_frame.h"
#include "process.h"
#include "pycode.h"
#include "pycompat.h"
#include "pyframe.h"
#include "pythread.h"
#include "pytypes.h"
#include "version.h"

namespace {

static const std::string PERM_MESSAGE = "Operation not permitted";

class DirectoryReader
{
  public:
    explicit DirectoryReader(const std::string& path)
    : dir_(opendir(path.c_str()))
    {
        if (!dir_) {
            throw std::runtime_error("Could not read the contents of " + path);
        }
    };

    ~DirectoryReader()
    {
        closedir(dir_);
    };

    std::vector<std::string> files() const
    {
        std::vector<std::string> files;
        struct dirent* ent;
        while ((ent = readdir(dir_)) != nullptr) {
            if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
                continue;
            }
            files.emplace_back(ent->d_name);
        }
        return files;
    }

  private:
    DIR* dir_;
};

}  // namespace
namespace pystack {

namespace {  // unnamed

struct ParsedPyVersion
{
    int major;
    int minor;
    int patch;
    const char* release_level;
    int serial;
};

std::ostream&
operator<<(std::ostream& out, const ParsedPyVersion& version)
{
    // Use a temporary stringstream in case `out` is using hex or showbase
    std::ostringstream oss;
    oss << version.major << "." << version.minor << "." << version.patch;
    if (version.release_level[0]) {
        oss << version.release_level << version.serial;
    }

    out << oss.str();
    return out;
}

bool
parsePyVersionHex(uint64_t version, ParsedPyVersion& parsed)
{
    int major = (version >> 24) & 0xFF;
    int minor = (version >> 16) & 0xFF;
    int patch = (version >> 8) & 0xFF;
    int level = (version >> 4) & 0x0F;
    int count = (version >> 0) & 0x0F;

    const char* level_str = "(unknown release level)";
    if (level == 0xA) {
        level_str = "a";
    } else if (level == 0xB) {
        level_str = "b";
    } else if (level == 0xC) {
        level_str = "rc";
    } else if (level == 0xF) {
        level_str = "";
    }

    if (major < 2 || major > 3 || level_str == nullptr || (level == 0xF && count != 0)) {
        return false;  // Doesn't look valid.
    }

    parsed = ParsedPyVersion{major, minor, patch, level_str, count};
    return true;
}

}  // unnamed namespace

static std::vector<int>
getProcessTids(pid_t pid)
{
    std::string filepath = "/proc/" + std::to_string(pid) + "/task";
    ::DirectoryReader reader(filepath);
    std::vector<std::string> files = reader.files();
    std::vector<int> tids;
    std::transform(
            files.cbegin(),
            files.cend(),
            std::back_inserter(tids),
            [](const std::string& file) -> int { return std::stoi(file); });
    return tids;
}

ProcessTracer::ProcessTracer(pid_t pid)
{
    std::unordered_map<int, int> error_by_tid;

    bool found_new_tid = true;
    while (found_new_tid) {
        found_new_tid = false;

        auto tids = getProcessTids(pid);
        for (auto& tid : tids) {
            if (d_tids.count(tid)) {
                continue;  // already stopped
            }

            auto err_it = error_by_tid.find(tid);
            if (err_it != error_by_tid.end()) {
                // We got an error for this TID on the last iteration.
                // Since we found the TID again this iteration, it still
                // belongs to us and should have been stoppable.
                detachFromProcess();

                int error = err_it->second;
                if (error == EPERM) {
                    throw std::runtime_error(PERM_MESSAGE);
                }
                throw std::system_error(error, std::generic_category());
            }

            found_new_tid = true;

            LOG(INFO) << "Trying to stop thread " << tid;
            long ret = ptrace(PTRACE_ATTACH, tid, nullptr, nullptr);
            if (ret < 0) {
                int error = errno;
                LOG(WARNING) << "Failed to attach to thread " << tid << ": " << strerror(error);
                error_by_tid.emplace(tid, error);
                continue;
            }

            // Add each tid as we attach: these are the tids we detach from.
            d_tids.insert(tid);

            LOG(INFO) << "Waiting for thread " << tid << " to be stopped";
            ret = waitpid(tid, nullptr, WUNTRACED);
            if (ret < 0) {
                // In some old kernels is not possible to use WUNTRACED with
                // threads (only the main thread will return a non zero value).
                if (tid == pid || errno != ECHILD) {
                    detachFromProcess();
                }
            }
            LOG(INFO) << "Thread " << tid << " stopped";
        }
    }
    LOG(INFO) << "All " << d_tids.size() << " threads stopped";
}

void
ProcessTracer::detachFromProcess()
{
    for (auto& tid : d_tids) {
        LOG(INFO) << "Detaching from thread " << tid;
        ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    }
}

ProcessTracer::~ProcessTracer()
{
    detachFromProcess();
}

std::vector<int>
ProcessTracer::getTids() const
{
    return {d_tids.begin(), d_tids.end()};
}

AbstractProcessManager::AbstractProcessManager(
        pid_t pid,
        std::vector<VirtualMap>&& memory_maps,
        MemoryMapInformation&& map_info)
: d_pid(pid)
, d_memory_maps(memory_maps)
, d_manager(nullptr)
, d_unwinder(nullptr)
, d_analyzer(nullptr)
{
    d_main_map = map_info.MainMap();
    d_bss = map_info.Bss();
    d_heap = map_info.Heap();
    if (!d_main_map) {
        throw std::runtime_error("The main interpreter map could not be located");
    }
}

bool
AbstractProcessManager::isValidDictionaryObject(remote_addr_t addr) const
{
    if (addr == (remote_addr_t) nullptr) {
        return false;
    }
    if (!isAddressValid(addr)) {
        return false;
    }
    try {
        Object pyobj(shared_from_this(), addr);
        return pyobj.objectType() == Object::ObjectType::DICT;
    } catch (RemoteMemCopyError& ex) {
        return false;
    }
}

bool
AbstractProcessManager::isValidInterpreterState(remote_addr_t addr) const
{
    /* The main idea here is that the PyInterpreterState has a pointer to the
    current thread state:

    typedef struct _is
    {
        struct PyThreadState *next;
        struct PyThreadState *tstate_head;
    } PyInterpreterState;

    and the PyThreadState has a pointer back to the PyInterpreterState:

    typedef struct PyThreadState
    {
        struct PyThreadState *next;
        PyInterpreterState *interp;
        ...
    }

    Using this information we can proceed as follows:

      - Interpret the memory region at the current position as PyInterpreterState.
      - Look at the address that the *tstate_head* member looks to, if the address
    does not look like garbage, copy the memory that the address points to from
    the remote process.
      - Reinterpret the memory we just copied as a PyThreadState and look at the
    address the *interp* member points to. This must point back to the address we
    started with, this is, the address of we are assuming that corresponds to a
    PyInterpreterState.
      - As a last security check: try to construct a single frame and the
    associated code object from the executing thread and check that the results
    make sense. We need to do this because, although very rare, there may be some
    random memory regions that have the previous properties but they are still
    garbage.

    If any of the previous steps fail, we continue with the next memory chunk
    until we find the PyInterpreterState or we run out of chunks.
    */
    if (!isAddressValid(addr)) {
        return false;
    }

    Structure<py_is_v> is(shared_from_this(), addr);
    // The check for valid addresses may fail if the address falls in the stack
    // space (there are "holes" in the address map space so just checking for
    // min_addr < addr < max_addr does not guarantee a valid address) so we need
    // to catch InvalidRemoteAddress exceptions.
    try {
        is.copyFromRemote();
    } catch (RemoteMemCopyError& ex) {
        return false;
    }

    auto current_thread_addr = is.getField(&py_is_v::o_tstate_head);
    if (!isAddressValid(current_thread_addr)) {
        return false;
    }

    Structure<py_thread_v> current_thread(shared_from_this(), current_thread_addr);
    try {
        current_thread.copyFromRemote();
    } catch (RemoteMemCopyError& ex) {
        return false;
    }

    if (current_thread.getField(&py_thread_v::o_interp) != addr) {
        return false;
    }

    LOG(DEBUG) << std::hex << std::showbase << "Possible PyInterpreterState candidate at address "
               << addr << " with tstate_head value of " << current_thread_addr;

    // Validate dictionaries in the interpreter state
    std::unordered_map<std::string, remote_addr_t> dictionaries(
            {{"modules", is.getField(&py_is_v::o_modules)},
             {"sysdict", is.getField(&py_is_v::o_sysdict)},
             {"builtins", is.getField(&py_is_v::o_builtins)}});
    for (const auto& [dictname, addr] : dictionaries) {
        if (!isValidDictionaryObject(addr)) {
            LOG(DEBUG) << "The '" << dictname << "' dictionary object is not valid";
            return false;
        }
        LOG(DEBUG) << "The '" << dictname << "' dictionary object is valid";
    }

    LOG(DEBUG) << std::hex << std::showbase << "Possible PyInterpreterState candidate at address "
               << addr << " is valid";

    return true;
}

remote_addr_t
AbstractProcessManager::findInterpreterStateFromPointer(remote_addr_t pointer) const
{
    LOG(DEBUG) << "Trying to determine PyInterpreterState directly from address " << std::hex
               << std::showbase << pointer;
    remote_addr_t interp_state;
    copyObjectFromProcess(pointer, &interp_state);
    if (!isValidInterpreterState(interp_state)) {
        LOG(INFO) << "Failed to determine PyInterpreterState directly from address " << std::hex
                  << std::showbase << pointer;
        return (remote_addr_t)NULL;
    }
    return interp_state;
}

remote_addr_t
AbstractProcessManager::findInterpreterStateFromPyRuntime(remote_addr_t runtime_addr) const
{
    LOG(INFO) << "Searching for PyInterpreterState based on PyRuntime address " << std::hex
              << std::showbase << runtime_addr;

    Structure<py_runtime_v> py_runtime(shared_from_this(), runtime_addr);
    remote_addr_t interp_state = py_runtime.getField(&py_runtime_v::o_interp_head);

    if (!isValidInterpreterState(interp_state)) {
        LOG(INFO) << "Failing to resolve PyInterpreterState based on PyRuntime address " << std::hex
                  << std::showbase << runtime_addr;
        return (remote_addr_t)NULL;
    }

    LOG(DEBUG) << "Interpreter head reference from symbol dereference successfully";
    return interp_state;
}

remote_addr_t
AbstractProcessManager::scanMemoryAreaForInterpreterState(const VirtualMap& map) const
{
    void* result = nullptr;
    size_t size = map.Size();
    std::vector<char> memory_buffer(size);
    remote_addr_t base = map.Start();
    copyMemoryFromProcess(base, size, memory_buffer.data());

    void* upper_bound = (void*)(memory_buffer.data() + size);

    LOG(INFO) << std::showbase << std::hex
              << "Searching for PyInterpreterState in memory area spanning from " << map.Start()
              << " to " << map.End();

    for (void** raddr = (void**)memory_buffer.data(); (void*)raddr < upper_bound; raddr++) {
        if (!isValidInterpreterState((remote_addr_t)*raddr)) {
            continue;
        }
        LOG(DEBUG) << std::hex << std::showbase
                   << "Possible interpreter state referenced by memory segment "
                   << reinterpret_cast<void*>((char*)raddr - (char*)memory_buffer.data() + (char*)base)
                   << " (offset " << reinterpret_cast<void*>((char*)raddr - (char*)memory_buffer.data())
                   << " ) -> addr " << static_cast<void*>((char*)raddr);
        result = *raddr;
        break;
    }
    if (result == nullptr) {
        LOG(INFO) << std::showbase << std::hex
                  << "Could not find a valid PyInterpreterState in memory area spanning from "
                  << map.Start() << " to " << map.End();
    }
    return (remote_addr_t)result;
}

remote_addr_t
AbstractProcessManager::scanMemoryAreaForDebugOffsets(const VirtualMap& map) const
{
    size_t size = map.Size();
    std::vector<char> memory_buffer(size);
    remote_addr_t base = map.Start();
    copyMemoryFromProcess(base, size, memory_buffer.data());

    LOG(INFO) << std::showbase << std::hex << "Searching for debug offsets in memory area spanning from "
              << map.Start() << " to " << map.End();

    uint64_t* lower_bound = (uint64_t*)&memory_buffer.data()[0];
    uint64_t* upper_bound = (uint64_t*)&memory_buffer.data()[size];

    uint64_t cookie;
    memcpy(&cookie, "xdebugpy", sizeof(cookie));

    for (uint64_t* raddr = lower_bound; raddr < upper_bound; raddr++) {
        if (raddr[0] == cookie) {
            uint64_t version = raddr[1];

            ParsedPyVersion parsed;
            if (parsePyVersionHex(version, parsed) && parsed.major == 3 && parsed.minor >= 13) {
                auto offset = (remote_addr_t)raddr - (remote_addr_t)memory_buffer.data();
                auto addr = offset + base;
                LOG(DEBUG) << std::hex << std::showbase << "Possible debug offsets found at address "
                           << addr << " in a mapping of " << map.Path();
                return addr;
            }
        }
    }
    return 0;
}

remote_addr_t
AbstractProcessManager::scanBSS() const
{
    LOG(INFO) << "Scanning BSS section for PyInterpreterState";
    if (!d_bss) {
        LOG(INFO) << "BSS analysis could not be performed because the BSS section is missing";
        return (remote_addr_t) nullptr;
    }
    return scanMemoryAreaForInterpreterState(d_bss.value());
}

remote_addr_t
AbstractProcessManager::scanAllAnonymousMaps() const
{
    LOG(INFO) << "Scanning all anonymous maps for PyInterpreterState";
    for (auto& map : d_memory_maps) {
        if (!map.Path().empty()) {
            continue;
        }
        LOG(DEBUG) << std::hex << std::showbase
                   << "Attempting to locate PyInterpreterState in with map starting at " << map.Start();
        remote_addr_t result = scanMemoryAreaForInterpreterState(map);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

remote_addr_t
AbstractProcessManager::scanHeap() const
{
    LOG(INFO) << "Scanning HEAP section for PyInterpreterState";
    if (!d_heap) {
        LOG(INFO) << "HEAP analysis could not be performed because the HEAP section is missing";
        return (remote_addr_t) nullptr;
    }
    return scanMemoryAreaForInterpreterState(d_heap.value());
}

remote_addr_t
AbstractProcessManager::findDebugOffsetsFromMaps() const
{
    LOG(INFO) << "Scanning all writable path-backed maps for _Py_DebugOffsets";
    for (auto& map : d_memory_maps) {
        if (map.Flags().find("w") != std::string::npos && !map.Path().empty()) {
            LOG(DEBUG) << std::hex << std::showbase << "Attempting to locate _Py_DebugOffsets in map of "
                       << map.Path() << " starting at " << map.Start() << " and ending at " << map.End();
            LOG(DEBUG) << "Flags: " << map.Flags();
            try {
                if (remote_addr_t result = scanMemoryAreaForDebugOffsets(map)) {
                    return result;
                }
            } catch (RemoteMemCopyError& ex) {
                LOG(INFO) << "Failed to scan map starting at " << map.Start();
            }
        }
    }
    return 0;
}

ssize_t
AbstractProcessManager::copyMemoryFromProcess(remote_addr_t addr, size_t size, void* destination) const
{
    return d_manager->copyMemoryFromProcess(addr, size, destination);
}

bool
AbstractProcessManager::isAddressValid(remote_addr_t addr) const
{
    return std::any_of(d_memory_maps.cbegin(), d_memory_maps.cend(), [&](const VirtualMap& map) {
        return d_manager->isAddressValid(addr, map);
    });
}

std::string
AbstractProcessManager::getStringFromAddress(remote_addr_t addr) const
{
    Python2::_PyStringObject string;
    std::vector<char> buffer;
    ssize_t len;
    remote_addr_t data_addr;

    if (d_major == 2) {
        LOG(DEBUG) << std::hex << std::showbase << "Handling string object of version 2 from address "
                   << addr;
        copyObjectFromProcess(addr, &string);

        len = string.ob_base.ob_size;
        buffer.resize(len);
        data_addr = (remote_addr_t)((char*)addr + offsetof(Python2::_PyStringObject, ob_sval));
        LOG(DEBUG) << std::hex << std::showbase << "Copying ASCII data for string object from address "
                   << data_addr;
        copyMemoryFromProcess(data_addr, len, buffer.data());
    } else {
        LOG(DEBUG) << std::hex << std::showbase << "Handling unicode object of version 3 from address "
                   << addr;
        Structure<py_unicode_v> unicode(shared_from_this(), addr);

        AnyPyUnicodeState state = unicode.getField(&py_unicode_v::o_state);
        if (versionIsAtLeast(3, 14) and isFreeThreaded()) {
            if (state.python3_14t.kind != 1 || state.python3_14t.compact != 1) {
                throw InvalidRemoteObject();
            }
        } else {
            if (state.python3.kind != 1 || state.python3.compact != 1) {
                throw InvalidRemoteObject();
            }
        }

        len = unicode.getField(&py_unicode_v::o_length);
        buffer.resize(len);
        data_addr = unicode.getFieldRemoteAddress(&py_unicode_v::o_ascii);
        LOG(DEBUG) << std::hex << std::showbase << "Copying ASCII data for unicode object from address "
                   << data_addr;
        copyMemoryFromProcess(data_addr, len, buffer.data());
    }
    return std::string(buffer.begin(), buffer.end());
}

// ----------------------------------------------------------------------------
std::string
AbstractProcessManager::getBytesFromAddress(remote_addr_t addr) const
{
    ssize_t len;
    std::vector<char> buffer;
    remote_addr_t data_addr;

    if (d_major == 2) {
        LOG(DEBUG) << std::hex << std::showbase << "Handling bytes object of version 2 from address "
                   << addr;
        Python2::_PyStringObject string;
        copyObjectFromProcess(addr, &string);
        len = string.ob_base.ob_size + 1;
        buffer.resize(len);
        data_addr = (remote_addr_t)((char*)addr + offsetof(Python2::_PyStringObject, ob_sval));
        LOG(DEBUG) << std::hex << std::showbase << "Copying data for bytes object from address "
                   << data_addr;
        copyMemoryFromProcess(data_addr, len, buffer.data());
    } else {
        LOG(DEBUG) << std::hex << std::showbase << "Handling bytes object of version 3 from address "
                   << addr;
        Structure<py_bytes_v> bytes(shared_from_this(), addr);
        len = bytes.getField(&py_bytes_v::o_ob_size) + 1;
        if (len < 1) {
            throw std::runtime_error("Incorrect size of the fetched bytes object");
        }
        buffer.resize(len);
        data_addr = bytes.getFieldRemoteAddress(&py_bytes_v::o_ob_sval);

        LOG(DEBUG) << std::hex << std::showbase << "Copying data for bytes object from address "
                   << data_addr;
        copyMemoryFromProcess(data_addr, len, buffer.data());
    }

    return std::string(buffer.begin(), buffer.end() - 1);
}

remote_addr_t
AbstractProcessManager::findSymbol(const std::string& symbol) const
{
    const auto elem = d_symbol_cache.find(symbol);
    if (elem == d_symbol_cache.cend()) {
        remote_addr_t addr = d_unwinder->getAddressforSymbol(symbol, d_main_map.value().Path());
        d_symbol_cache.emplace(symbol, addr);
        return addr;
    }
    return elem->second;
}

remote_addr_t
AbstractProcessManager::findInterpreterStateFromSymbols() const
{
    LOG(INFO) << "Trying to find PyInterpreterState with symbols";

    remote_addr_t pyruntime = findSymbol("_PyRuntime");
    if (pyruntime) {
        return findInterpreterStateFromPyRuntime(pyruntime);
    }

    // Older versions have a pointer to PyinterpreterState in "interp_head"
    remote_addr_t interp_head = findSymbol("interp_head");
    if (interp_head) {
        return findInterpreterStateFromPointer(interp_head);
    }
    return 0;
}

std::vector<NativeFrame>
AbstractProcessManager::unwindThread(pid_t tid) const
{
    return d_unwinder->unwindThread(tid);
}

pid_t
AbstractProcessManager::Pid() const
{
    return d_pid;
}

remote_addr_t
AbstractProcessManager::getAddressFromCache(const std::string& symbol) const
{
    return d_type_cache[symbol];
}

void
AbstractProcessManager::registerAddressInCache(const std::string& symbol, remote_addr_t address) const
{
    d_type_cache[symbol] = address;
}

std::string
AbstractProcessManager::getCStringFromAddress(remote_addr_t addr) const
{
    std::vector<char> result;
    char character = 0;
    size_t position = 0;
    do {
        copyObjectFromProcess(addr + ((position++) * sizeof(char)), &character);
        result.push_back(character);
    } while (character != 0);
    return std::string(result.cbegin(), result.cend() - 1);
}

AbstractProcessManager::InterpreterStatus
AbstractProcessManager::isInterpreterActive() const
{
    remote_addr_t runtime_addr = findSymbol("_PyRuntime");
    if (runtime_addr) {
        Structure<py_runtime_v> py_runtime(shared_from_this(), runtime_addr);
        remote_addr_t p = py_runtime.getField(&py_runtime_v::o_finalizing);
        return p == 0 ? InterpreterStatus::RUNNING : InterpreterStatus::FINALIZED;
    }

    return InterpreterStatus::UNKNOWN;
}

void
AbstractProcessManager::setPythonVersionFromDebugOffsets()
{
    remote_addr_t pyruntime_addr = findSymbol("_PyRuntime");
    if (!pyruntime_addr) {
        pyruntime_addr = findPyRuntimeFromElfData();
    }
    if (!pyruntime_addr) {
        pyruntime_addr = findDebugOffsetsFromMaps();
    }

    if (!pyruntime_addr) {
        LOG(DEBUG) << "Unable to find _Py_DebugOffsets";
        return;
    }

    try {
        uint64_t cookie;
        copyObjectFromProcess(pyruntime_addr, &cookie);
        if (0 != memcmp(&cookie, "xdebugpy", 8)) {
            LOG(DEBUG) << "Found a _PyRuntime structure without _Py_DebugOffsets";
            return;
        }

        uint64_t version;
        copyObjectFromProcess(pyruntime_addr + 8, &version);

        ParsedPyVersion parsed;
        if (parsePyVersionHex(version, parsed) && parsed.major == 3 && parsed.minor >= 13) {
            LOG(INFO) << std::hex << std::showbase << "_Py_DebugOffsets at " << pyruntime_addr
                      << " identify the version as " << parsed;
            setPythonVersion(std::make_pair(parsed.major, parsed.minor));
            Structure<py_runtime_v> py_runtime(shared_from_this(), pyruntime_addr);
            bool is_free_threaded = py_runtime.getField(&py_runtime_v::o_dbg_off_free_threaded);
            std::unique_ptr<python_v> offsets = loadDebugOffsets(py_runtime);
            if (offsets) {
                LOG(INFO) << "_Py_DebugOffsets appear to be valid and will be used";
                warnIfOffsetsAreMismatched(pyruntime_addr);
                d_debug_offsets_addr = pyruntime_addr;
                d_debug_offsets = std::move(offsets);
                d_is_free_threaded = is_free_threaded;
                return;
            }
        }
    } catch (const RemoteMemCopyError& ex) {
        LOG(DEBUG) << std::hex << std::showbase << "Found apparently invalid _Py_DebugOffsets at "
                   << pyruntime_addr;
    }

    LOG(DEBUG) << "Failed to validate _PyDebugOffsets structure";
    d_major = 0;
    d_minor = 0;
    d_py_v = nullptr;
    d_debug_offsets_addr = 0;
    d_debug_offsets.reset();
}

std::pair<int, int>
AbstractProcessManager::findPythonVersion() const
{
    if (d_py_v) {
        // Already set or previously found (probably via _Py_DebugOffsets)
        return std::make_pair(d_major, d_minor);
    }

    auto version_symbol = findSymbol("Py_Version");
    if (!version_symbol) {
        LOG(DEBUG) << "Failed to determine Python version from symbols";
        return {-1, -1};
    }
    unsigned long version;
    try {
        copyObjectFromProcess(version_symbol, &version);
    } catch (RemoteMemCopyError& ex) {
        LOG(DEBUG) << "Failed to determine Python version from symbols";
        return {-1, -1};
    }
    int major = (version >> 24) & 0xFF;
    int minor = (version >> 16) & 0xFF;
    int level = (version >> 4) & 0x0F;

    if (major == 0 && minor == 0) {
        LOG(DEBUG) << "Failed to determine Python version from symbols: empty data copied";
        return {-1, -1};
    }

    if (major != 2 && major != 3) {
        LOG(DEBUG) << "Failed to determine Python version from symbols: invalid major version";
        return {-1, -1};
    }

    if (level != 0xA && level != 0xB && level != 0xC && level != 0xF) {
        LOG(DEBUG) << "Failed to determine Python version from symbols: invalid release level";
        return {-1, -1};
    }

    LOG(DEBUG) << "Python version determined from symbols: " << major << "." << minor;
    return {major, minor};
}

void
AbstractProcessManager::setPythonVersion(const std::pair<int, int>& version)
{
    d_py_v = getCPythonOffsets(version.first, version.second);
    // Note: getCPythonOffsets can throw. Don't set these if it does.
    d_major = version.first;
    d_minor = version.second;
}

void
AbstractProcessManager::warnIfOffsetsAreMismatched(remote_addr_t runtime_addr) const
{
    Structure<py_runtime_v> py_runtime(shared_from_this(), runtime_addr);

    if (0 != memcmp(py_runtime.getField(&py_runtime_v::o_dbg_off_cookie), "xdebugpy", 8)) {
        LOG(WARNING) << "Debug offsets cookie doesn't match!";
        return;
    }

    // Note: It's OK for pystack's size to be smaller, but not larger.
#define compare_size(size_offset, pystack_struct)                                                       \
    if ((d_py_v->py_runtime.*size_offset).offset                                                        \
        && ((uint64_t)offsets().pystack_struct.size > py_runtime.getField(size_offset)))                \
    {                                                                                                   \
        LOG(INFO) << "Debug offsets mismatch: compiled-in " << sizeof(void*) * 8 << "-bit python3."     \
                  << d_minor << " " #pystack_struct ".size " << offsets().pystack_struct.size << " > "  \
                  << py_runtime.getField(size_offset) << " loaded from _Py_DebugOffsets";               \
    } else                                                                                              \
        do {                                                                                            \
        } while (0)

#define compare_offset(field_offset_offset, pystack_field)                                              \
    if ((d_py_v->py_runtime.*field_offset_offset).offset                                                \
        && (uint64_t)offsets().pystack_field.offset != py_runtime.getField(field_offset_offset))        \
    {                                                                                                   \
        LOG(INFO) << "Debug offsets mismatch: compiled-in " << sizeof(void*) * 8 << "-bit python3."     \
                  << d_minor << " " #pystack_field << " " << offsets().pystack_field.offset             \
                  << " != " << py_runtime.getField(field_offset_offset)                                 \
                  << " loaded from _Py_DebugOffsets";                                                   \
    } else                                                                                              \
        do {                                                                                            \
        } while (0)

    compare_size(&py_runtime_v::o_dbg_off_runtime_state_struct_size, py_runtime);
    compare_offset(&py_runtime_v::o_dbg_off_runtime_state_finalizing, py_runtime.o_finalizing);
    compare_offset(&py_runtime_v::o_dbg_off_runtime_state_interpreters_head, py_runtime.o_interp_head);

    compare_size(&py_runtime_v::o_dbg_off_interpreter_state_struct_size, py_is);
    compare_offset(&py_runtime_v::o_dbg_off_interpreter_state_next, py_is.o_next);
    compare_offset(&py_runtime_v::o_dbg_off_interpreter_state_threads_head, py_is.o_tstate_head);
    compare_offset(&py_runtime_v::o_dbg_off_interpreter_state_gc, py_is.o_gc);
    compare_offset(&py_runtime_v::o_dbg_off_interpreter_state_imports_modules, py_is.o_modules);
    compare_offset(&py_runtime_v::o_dbg_off_interpreter_state_sysdict, py_is.o_sysdict);
    compare_offset(&py_runtime_v::o_dbg_off_interpreter_state_builtins, py_is.o_builtins);
    compare_offset(&py_runtime_v::o_dbg_off_interpreter_state_ceval_gil, py_is.o_gil_runtime_state);

    compare_size(&py_runtime_v::o_dbg_off_thread_state_struct_size, py_thread);
    compare_offset(&py_runtime_v::o_dbg_off_thread_state_prev, py_thread.o_prev);
    compare_offset(&py_runtime_v::o_dbg_off_thread_state_next, py_thread.o_next);
    compare_offset(&py_runtime_v::o_dbg_off_thread_state_interp, py_thread.o_interp);
    compare_offset(&py_runtime_v::o_dbg_off_thread_state_current_frame, py_thread.o_frame);
    compare_offset(&py_runtime_v::o_dbg_off_thread_state_thread_id, py_thread.o_thread_id);
    compare_offset(&py_runtime_v::o_dbg_off_thread_state_native_thread_id, py_thread.o_native_thread_id);

    compare_size(&py_runtime_v::o_dbg_off_interpreter_frame_struct_size, py_frame);
    compare_offset(&py_runtime_v::o_dbg_off_interpreter_frame_previous, py_frame.o_back);
    compare_offset(&py_runtime_v::o_dbg_off_interpreter_frame_executable, py_frame.o_code);
    compare_offset(&py_runtime_v::o_dbg_off_interpreter_frame_instr_ptr, py_frame.o_prev_instr);
    compare_offset(&py_runtime_v::o_dbg_off_interpreter_frame_localsplus, py_frame.o_localsplus);
    compare_offset(&py_runtime_v::o_dbg_off_interpreter_frame_owner, py_frame.o_owner);

    compare_size(&py_runtime_v::o_dbg_off_code_object_struct_size, py_code);
    compare_offset(&py_runtime_v::o_dbg_off_code_object_filename, py_code.o_filename);
    compare_offset(&py_runtime_v::o_dbg_off_code_object_name, py_code.o_name);
    compare_offset(&py_runtime_v::o_dbg_off_code_object_linetable, py_code.o_lnotab);
    compare_offset(&py_runtime_v::o_dbg_off_code_object_firstlineno, py_code.o_firstlineno);
    compare_offset(&py_runtime_v::o_dbg_off_code_object_argcount, py_code.o_argcount);
    compare_offset(&py_runtime_v::o_dbg_off_code_object_localsplusnames, py_code.o_varnames);
    compare_offset(&py_runtime_v::o_dbg_off_code_object_co_code_adaptive, py_code.o_code_adaptive);

    compare_size(&py_runtime_v::o_dbg_off_pyobject_struct_size, py_object);
    compare_offset(&py_runtime_v::o_dbg_off_pyobject_ob_type, py_object.o_ob_type);

    compare_size(&py_runtime_v::o_dbg_off_type_object_struct_size, py_type);
    compare_offset(&py_runtime_v::o_dbg_off_type_object_tp_name, py_type.o_tp_name);
    compare_offset(&py_runtime_v::o_dbg_off_type_object_tp_repr, py_type.o_tp_repr);
    compare_offset(&py_runtime_v::o_dbg_off_type_object_tp_flags, py_type.o_tp_flags);

    compare_size(&py_runtime_v::o_dbg_off_tuple_object_struct_size, py_tuple);
    compare_offset(&py_runtime_v::o_dbg_off_tuple_object_ob_item, py_tuple.o_ob_item);
    compare_offset(&py_runtime_v::o_dbg_off_tuple_object_ob_size, py_tuple.o_ob_size);

    compare_size(&py_runtime_v::o_dbg_off_list_object_struct_size, py_list);
    compare_offset(&py_runtime_v::o_dbg_off_list_object_ob_item, py_list.o_ob_item);
    compare_offset(&py_runtime_v::o_dbg_off_list_object_ob_size, py_list.o_ob_size);

    compare_size(&py_runtime_v::o_dbg_off_dict_object_struct_size, py_dict);
    compare_offset(&py_runtime_v::o_dbg_off_dict_object_ma_keys, py_dict.o_ma_keys);
    compare_offset(&py_runtime_v::o_dbg_off_dict_object_ma_values, py_dict.o_ma_values);

    compare_size(&py_runtime_v::o_dbg_off_float_object_struct_size, py_float);
    compare_offset(&py_runtime_v::o_dbg_off_float_object_ob_fval, py_float.o_ob_fval);

    compare_size(&py_runtime_v::o_dbg_off_long_object_struct_size, py_long);
    compare_offset(&py_runtime_v::o_dbg_off_long_object_lv_tag, py_long.o_ob_size);
    compare_offset(&py_runtime_v::o_dbg_off_long_object_ob_digit, py_long.o_ob_digit);

    compare_size(&py_runtime_v::o_dbg_off_bytes_object_struct_size, py_bytes);
    compare_offset(&py_runtime_v::o_dbg_off_bytes_object_ob_size, py_bytes.o_ob_size);
    compare_offset(&py_runtime_v::o_dbg_off_bytes_object_ob_sval, py_bytes.o_ob_sval);

    compare_size(&py_runtime_v::o_dbg_off_unicode_object_struct_size, py_unicode);
    compare_offset(&py_runtime_v::o_dbg_off_unicode_object_state, py_unicode.o_state);
    compare_offset(&py_runtime_v::o_dbg_off_unicode_object_length, py_unicode.o_length);
    compare_offset(&py_runtime_v::o_dbg_off_unicode_object_asciiobject_size, py_unicode.o_ascii);

    compare_size(&py_runtime_v::o_dbg_off_gc_struct_size, py_gc);
    compare_offset(&py_runtime_v::o_dbg_off_gc_collecting, py_gc.o_collecting);

#undef compare_size
#undef compare_offset
}

std::unique_ptr<python_v>
AbstractProcessManager::loadDebugOffsets(Structure<py_runtime_v>& py_runtime) const
{
    if (!versionIsAtLeast(3, 13)) {
        return {};  // _Py_DebugOffsets was added in 3.13
    }

    if (0 != memcmp(py_runtime.getField(&py_runtime_v::o_dbg_off_cookie), "xdebugpy", 8)) {
        LOG(WARNING) << "Debug offsets cookie doesn't match!";
        return {};
    }

    uint64_t version = py_runtime.getField(&py_runtime_v::o_dbg_off_py_version_hex);
    int major = (version >> 24) & 0xff;
    int minor = (version >> 16) & 0xff;

    if (major != d_major || minor != d_minor) {
        LOG(WARNING) << "Detected version " << d_major << "." << d_minor
                     << " doesn't match debug offsets version " << major << "." << minor << "!";
        return {};
    }

    python_v debug_offsets{};
    if (!copyDebugOffsets(py_runtime, debug_offsets)) {
        return {};
    }

    if (!validateDebugOffsets(py_runtime, debug_offsets)) {
        return {};
    }

    auto ret = std::make_unique<python_v>();
    *ret = debug_offsets;
    clampSizes(*ret);
    return ret;
}

bool
AbstractProcessManager::copyDebugOffsets(Structure<py_runtime_v>& py_runtime, python_v& debug_offsets)
        const
{
    // Fill in a temporary python_v with the offsets from the remote. For fields
    // that aren't in _Py_DebugOffsets, assume our static offsets are correct.

#define set_size(pystack_struct, size_offset)                                                           \
    debug_offsets.pystack_struct.size = py_runtime.getField(size_offset)

#define set_offset(pystack_field, field_offset_offset)                                                  \
    debug_offsets.pystack_field = {(offset_t)py_runtime.getField(field_offset_offset)}

    set_size(py_runtime, &py_runtime_v::o_dbg_off_runtime_state_struct_size);
    set_offset(py_runtime.o_finalizing, &py_runtime_v::o_dbg_off_runtime_state_finalizing);
    set_offset(py_runtime.o_interp_head, &py_runtime_v::o_dbg_off_runtime_state_interpreters_head);

    set_size(py_is, &py_runtime_v::o_dbg_off_interpreter_state_struct_size);
    set_offset(py_is.o_next, &py_runtime_v::o_dbg_off_interpreter_state_next);
    set_offset(py_is.o_tstate_head, &py_runtime_v::o_dbg_off_interpreter_state_threads_head);
    set_offset(py_is.o_gc, &py_runtime_v::o_dbg_off_interpreter_state_gc);
    set_offset(py_is.o_modules, &py_runtime_v::o_dbg_off_interpreter_state_imports_modules);
    set_offset(py_is.o_sysdict, &py_runtime_v::o_dbg_off_interpreter_state_sysdict);
    set_offset(py_is.o_builtins, &py_runtime_v::o_dbg_off_interpreter_state_builtins);
    set_offset(py_is.o_gil_runtime_state, &py_runtime_v::o_dbg_off_interpreter_state_ceval_gil);
    set_offset(py_is.o_id, &py_runtime_v::o_dbg_off_interpreter_state_id);

    set_size(py_thread, &py_runtime_v::o_dbg_off_thread_state_struct_size);
    set_offset(py_thread.o_prev, &py_runtime_v::o_dbg_off_thread_state_prev);
    set_offset(py_thread.o_next, &py_runtime_v::o_dbg_off_thread_state_next);
    set_offset(py_thread.o_interp, &py_runtime_v::o_dbg_off_thread_state_interp);
    set_offset(py_thread.o_frame, &py_runtime_v::o_dbg_off_thread_state_current_frame);
    set_offset(py_thread.o_thread_id, &py_runtime_v::o_dbg_off_thread_state_thread_id);
    set_offset(py_thread.o_native_thread_id, &py_runtime_v::o_dbg_off_thread_state_native_thread_id);

    set_size(py_frame, &py_runtime_v::o_dbg_off_interpreter_frame_struct_size);
    set_offset(py_frame.o_back, &py_runtime_v::o_dbg_off_interpreter_frame_previous);
    set_offset(py_frame.o_code, &py_runtime_v::o_dbg_off_interpreter_frame_executable);
    set_offset(py_frame.o_prev_instr, &py_runtime_v::o_dbg_off_interpreter_frame_instr_ptr);
    set_offset(py_frame.o_localsplus, &py_runtime_v::o_dbg_off_interpreter_frame_localsplus);
    set_offset(py_frame.o_owner, &py_runtime_v::o_dbg_off_interpreter_frame_owner);

    set_size(py_code, &py_runtime_v::o_dbg_off_code_object_struct_size);
    set_offset(py_code.o_filename, &py_runtime_v::o_dbg_off_code_object_filename);
    set_offset(py_code.o_name, &py_runtime_v::o_dbg_off_code_object_name);
    set_offset(py_code.o_lnotab, &py_runtime_v::o_dbg_off_code_object_linetable);
    set_offset(py_code.o_firstlineno, &py_runtime_v::o_dbg_off_code_object_firstlineno);
    set_offset(py_code.o_argcount, &py_runtime_v::o_dbg_off_code_object_argcount);
    set_offset(py_code.o_varnames, &py_runtime_v::o_dbg_off_code_object_localsplusnames);
    set_offset(py_code.o_code_adaptive, &py_runtime_v::o_dbg_off_code_object_co_code_adaptive);

    set_size(py_object, &py_runtime_v::o_dbg_off_pyobject_struct_size);
    set_offset(py_object.o_ob_type, &py_runtime_v::o_dbg_off_pyobject_ob_type);

    set_size(py_type, &py_runtime_v::o_dbg_off_type_object_struct_size);
    set_offset(py_type.o_tp_name, &py_runtime_v::o_dbg_off_type_object_tp_name);
    set_offset(py_type.o_tp_repr, &py_runtime_v::o_dbg_off_type_object_tp_repr);
    set_offset(py_type.o_tp_flags, &py_runtime_v::o_dbg_off_type_object_tp_flags);

    set_size(py_tuple, &py_runtime_v::o_dbg_off_tuple_object_struct_size);
    set_offset(py_tuple.o_ob_item, &py_runtime_v::o_dbg_off_tuple_object_ob_item);
    set_offset(py_tuple.o_ob_size, &py_runtime_v::o_dbg_off_tuple_object_ob_size);

    set_size(py_list, &py_runtime_v::o_dbg_off_list_object_struct_size);
    set_offset(py_list.o_ob_item, &py_runtime_v::o_dbg_off_list_object_ob_item);
    set_offset(py_list.o_ob_size, &py_runtime_v::o_dbg_off_list_object_ob_size);

    set_size(py_dict, &py_runtime_v::o_dbg_off_dict_object_struct_size);
    set_offset(py_dict.o_ma_keys, &py_runtime_v::o_dbg_off_dict_object_ma_keys);
    set_offset(py_dict.o_ma_values, &py_runtime_v::o_dbg_off_dict_object_ma_values);

    // Assume our static offsets for dict keys and values are correct
    debug_offsets.py_dictkeys = d_py_v->py_dictkeys;
    debug_offsets.py_dictvalues = d_py_v->py_dictvalues;

    set_size(py_float, &py_runtime_v::o_dbg_off_float_object_struct_size);
    set_offset(py_float.o_ob_fval, &py_runtime_v::o_dbg_off_float_object_ob_fval);

    set_size(py_long, &py_runtime_v::o_dbg_off_long_object_struct_size);
    set_offset(py_long.o_ob_size, &py_runtime_v::o_dbg_off_long_object_lv_tag);
    set_offset(py_long.o_ob_digit, &py_runtime_v::o_dbg_off_long_object_ob_digit);

    set_size(py_bytes, &py_runtime_v::o_dbg_off_bytes_object_struct_size);
    set_offset(py_bytes.o_ob_size, &py_runtime_v::o_dbg_off_bytes_object_ob_size);
    set_offset(py_bytes.o_ob_sval, &py_runtime_v::o_dbg_off_bytes_object_ob_sval);

    set_size(py_unicode, &py_runtime_v::o_dbg_off_unicode_object_struct_size);
    set_offset(py_unicode.o_state, &py_runtime_v::o_dbg_off_unicode_object_state);
    set_offset(py_unicode.o_length, &py_runtime_v::o_dbg_off_unicode_object_length);
    set_offset(py_unicode.o_ascii, &py_runtime_v::o_dbg_off_unicode_object_asciiobject_size);

    set_size(py_gc, &py_runtime_v::o_dbg_off_gc_struct_size);
    set_offset(py_gc.o_collecting, &py_runtime_v::o_dbg_off_gc_collecting);

    // Assume our static offsets for cframe are all correct
    debug_offsets.py_cframe = d_py_v->py_cframe;

    size_t gilruntimestate_start =
            py_runtime.getField(&py_runtime_v::o_dbg_off_interpreter_state_gil_runtime_state);

    size_t gilruntimestate_locked_offset =
            py_runtime.getField(&py_runtime_v::o_dbg_off_interpreter_state_gil_runtime_state_locked)
            - gilruntimestate_start;
    size_t gilruntimestate_holder_offset =
            py_runtime.getField(&py_runtime_v::o_dbg_off_interpreter_state_gil_runtime_state_holder)
            - gilruntimestate_start;

    debug_offsets.py_gilruntimestate.size = std::max(
            {gilruntimestate_locked_offset + sizeof(decltype(py_gilruntimestate_v::o_locked)::Type),
             gilruntimestate_holder_offset
                     + sizeof(decltype(py_gilruntimestate_v::o_last_holder)::Type)});
    debug_offsets.py_gilruntimestate.o_locked.offset = gilruntimestate_locked_offset;
    debug_offsets.py_gilruntimestate.o_last_holder.offset = gilruntimestate_holder_offset;

#undef set_size
#undef set_offset

    return true;
}

bool
AbstractProcessManager::validateDebugOffsets(
        const Structure<py_runtime_v>& py_runtime,
        python_v& debug_offsets) const
{
    // Simple sanity checks on the decoded offsets:
    // - No structure is larger than 1 MB
    // - Every field falls within its structure's size
#define check_size(pystack_struct, size_offset)                                                         \
    do {                                                                                                \
        if (debug_offsets.pystack_struct.size > 1024 * 1024) {                                          \
            LOG(WARNING) << "Ignoring debug offsets because " #pystack_struct ".size ("                 \
                         << debug_offsets.pystack_struct.size << ") reported at byte offset "           \
                         << (d_py_v->py_runtime.*size_offset).offset                                    \
                         << " in detected _Py_DebugOffsets structure at " << std::hex << std::showbase  \
                         << py_runtime.getFieldRemoteAddress(&py_runtime_v::o_dbg_off_cookie)           \
                         << " is implausibly large";                                                    \
            return {};                                                                                  \
        }                                                                                               \
    } while (0)

#define check_field_bounds(structure, field)                                                            \
    do {                                                                                                \
        if (debug_offsets.structure.size < 0                                                            \
            || (size_t)debug_offsets.structure.size < debug_offsets.structure.field.offset              \
            || debug_offsets.structure.size - debug_offsets.structure.field.offset                      \
                       < sizeof(decltype(debug_offsets.structure.field)::Type))                         \
        {                                                                                               \
            LOG(WARNING) << "Ignoring debug offsets because " #structure ".size ("                      \
                         << debug_offsets.structure.size << ") - " #structure "." #field ".offset ("    \
                         << debug_offsets.structure.field.offset << ") < the field's size ("            \
                         << sizeof(decltype(debug_offsets.structure.field)::Type) << ")";               \
            return {};                                                                                  \
        }                                                                                               \
    } while (0)

    check_size(py_runtime, &py_runtime_v::o_dbg_off_runtime_state_struct_size);
    check_field_bounds(py_runtime, o_finalizing);
    check_field_bounds(py_runtime, o_interp_head);

    check_size(py_is, &py_runtime_v::o_dbg_off_interpreter_state_struct_size);
    check_field_bounds(py_is, o_next);
    check_field_bounds(py_is, o_tstate_head);
    check_field_bounds(py_is, o_gc);
    check_field_bounds(py_is, o_modules);
    check_field_bounds(py_is, o_sysdict);
    check_field_bounds(py_is, o_builtins);
    check_field_bounds(py_is, o_gil_runtime_state);

    check_size(py_thread, &py_runtime_v::o_dbg_off_thread_state_struct_size);
    check_field_bounds(py_thread, o_prev);
    check_field_bounds(py_thread, o_next);
    check_field_bounds(py_thread, o_interp);
    check_field_bounds(py_thread, o_frame);
    check_field_bounds(py_thread, o_thread_id);
    check_field_bounds(py_thread, o_native_thread_id);

    check_size(py_frame, &py_runtime_v::o_dbg_off_interpreter_frame_struct_size);
    check_field_bounds(py_frame, o_back);
    check_field_bounds(py_frame, o_code);
    check_field_bounds(py_frame, o_prev_instr);
    check_field_bounds(py_frame, o_localsplus);
    check_field_bounds(py_frame, o_owner);

    check_size(py_code, &py_runtime_v::o_dbg_off_code_object_struct_size);
    check_field_bounds(py_code, o_filename);
    check_field_bounds(py_code, o_name);
    check_field_bounds(py_code, o_lnotab);
    check_field_bounds(py_code, o_firstlineno);
    check_field_bounds(py_code, o_argcount);
    check_field_bounds(py_code, o_varnames);
    check_field_bounds(py_code, o_code_adaptive);

    check_size(py_object, &py_runtime_v::o_dbg_off_pyobject_struct_size);
    check_field_bounds(py_object, o_ob_type);

    check_size(py_type, &py_runtime_v::o_dbg_off_type_object_struct_size);
    check_field_bounds(py_type, o_tp_name);
    check_field_bounds(py_type, o_tp_repr);
    check_field_bounds(py_type, o_tp_flags);

    check_size(py_tuple, &py_runtime_v::o_dbg_off_tuple_object_struct_size);
    check_field_bounds(py_tuple, o_ob_size);
    check_field_bounds(py_tuple, o_ob_item);

    check_size(py_unicode, &py_runtime_v::o_dbg_off_unicode_object_struct_size);
    check_field_bounds(py_unicode, o_state);
    check_field_bounds(py_unicode, o_length);
    check_field_bounds(py_unicode, o_ascii);

    check_size(py_gc, &py_runtime_v::o_dbg_off_gc_struct_size);
    check_field_bounds(py_gc, o_collecting);

    check_field_bounds(py_list, o_ob_size);
    check_field_bounds(py_list, o_ob_item);

    check_field_bounds(py_dictkeys, o_dk_size);
    check_field_bounds(py_dictkeys, o_dk_kind);
    check_field_bounds(py_dictkeys, o_dk_nentries);
    check_field_bounds(py_dictkeys, o_dk_indices);

    check_field_bounds(py_dictvalues, o_values);

    check_field_bounds(py_dict, o_ma_keys);
    check_field_bounds(py_dict, o_ma_values);

    check_field_bounds(py_float, o_ob_fval);

    check_field_bounds(py_long, o_ob_size);
    check_field_bounds(py_long, o_ob_digit);

    check_field_bounds(py_bytes, o_ob_size);
    check_field_bounds(py_bytes, o_ob_sval);

    check_field_bounds(py_cframe, current_frame);

#undef check_size
#undef check_field_bounds

    return true;
}

void
AbstractProcessManager::clampSizes(python_v& debug_offsets) const
{
    // Clamp the size of each struct down to only what we need to copy.
    // The runtime state and interpreter state both contain many fields beyond
    // the ones that we're interested in or have offsets for.
#define update_size(structure, field)                                                                   \
    debug_offsets.structure.size = std::max(                                                            \
            (size_t)debug_offsets.structure.size,                                                       \
            debug_offsets.structure.field.offset                                                        \
                    + sizeof(decltype(debug_offsets.structure.field)::Type))

    debug_offsets.py_runtime.size = 0;
    update_size(py_runtime, o_finalizing);
    update_size(py_runtime, o_interp_head);

    debug_offsets.py_is.size = 0;
    update_size(py_is, o_next);
    update_size(py_is, o_tstate_head);
    update_size(py_is, o_gc);
    update_size(py_is, o_modules);
    update_size(py_is, o_sysdict);
    update_size(py_is, o_builtins);
    update_size(py_is, o_gil_runtime_state);

    debug_offsets.py_thread.size = 0;
    update_size(py_thread, o_prev);
    update_size(py_thread, o_next);
    update_size(py_thread, o_interp);
    update_size(py_thread, o_frame);
    update_size(py_thread, o_thread_id);
    update_size(py_thread, o_native_thread_id);

    debug_offsets.py_frame.size = 0;
    update_size(py_frame, o_back);
    update_size(py_frame, o_code);
    update_size(py_frame, o_prev_instr);
    update_size(py_frame, o_localsplus);
    update_size(py_frame, o_owner);

    debug_offsets.py_code.size = 0;
    update_size(py_code, o_filename);
    update_size(py_code, o_name);
    update_size(py_code, o_lnotab);
    update_size(py_code, o_firstlineno);
    update_size(py_code, o_argcount);
    update_size(py_code, o_varnames);
    update_size(py_code, o_code_adaptive);

    debug_offsets.py_object.size = 0;
    update_size(py_object, o_ob_type);

    debug_offsets.py_type.size = 0;
    update_size(py_type, o_tp_name);
    update_size(py_type, o_tp_repr);
    update_size(py_type, o_tp_flags);

    debug_offsets.py_tuple.size = 0;
    update_size(py_tuple, o_ob_size);
    update_size(py_tuple, o_ob_item);

    debug_offsets.py_unicode.size = 0;
    update_size(py_unicode, o_state);
    update_size(py_unicode, o_length);
    update_size(py_unicode, o_ascii);

    debug_offsets.py_gc.size = 0;
    update_size(py_gc, o_collecting);

    debug_offsets.py_list.size = 0;
    update_size(py_list, o_ob_size);
    update_size(py_list, o_ob_item);

    debug_offsets.py_dictkeys.size = 0;
    update_size(py_dictkeys, o_dk_size);
    update_size(py_dictkeys, o_dk_kind);
    update_size(py_dictkeys, o_dk_nentries);
    update_size(py_dictkeys, o_dk_indices);

    debug_offsets.py_dictvalues.size = 0;
    update_size(py_dictvalues, o_values);

    debug_offsets.py_dict.size = 0;
    update_size(py_dict, o_ma_keys);
    update_size(py_dict, o_ma_values);

    debug_offsets.py_float.size = 0;
    update_size(py_float, o_ob_fval);

    debug_offsets.py_long.size = 0;
    update_size(py_long, o_ob_size);
    update_size(py_long, o_ob_digit);

    debug_offsets.py_bytes.size = 0;
    update_size(py_bytes, o_ob_size);
    update_size(py_bytes, o_ob_sval);

    debug_offsets.py_cframe.size = 0;
    update_size(py_cframe, current_frame);
}

bool
AbstractProcessManager::versionIsAtLeast(int required_major, int required_minor) const
{
    return d_major > required_major || (d_major == required_major && d_minor >= required_minor);
}

bool
AbstractProcessManager::isFreeThreaded() const
{
    return d_is_free_threaded;
}

const python_v&
AbstractProcessManager::offsets() const
{
    if (d_debug_offsets) {
        return *d_debug_offsets;
    }
    return *d_py_v;
}

remote_addr_t
AbstractProcessManager::findPyRuntimeFromElfData() const
{
    LOG(INFO) << "Trying to resolve PyInterpreterState from Elf data";
    SectionInfo section_info;
    if (!getSectionInfo(d_main_map.value().Path(), ".PyRuntime", &section_info)) {
        LOG(INFO) << "Failed to resolve PyInterpreterState from Elf data because .PyRuntime section "
                     "could not be found";
        return 0;
    }
    remote_addr_t load_addr = getLoadPointOfModule(d_analyzer->getDwfl(), d_main_map.value().Path());
    if (load_addr == 0) {
        LOG(INFO) << "Failed to resolve PyInterpreterState from Elf data because module load point "
                     "could not be found";
        return 0;
    }
    return load_addr + section_info.corrected_addr;
}

remote_addr_t
AbstractProcessManager::findInterpreterStateFromElfData() const
{
    remote_addr_t pyruntime = findPyRuntimeFromElfData();
    if (!pyruntime) {
        return 0;
    }
    return findInterpreterStateFromPyRuntime(pyruntime);
}

remote_addr_t
AbstractProcessManager::findInterpreterStateFromDebugOffsets() const
{
    if (!d_debug_offsets_addr) {
        LOG(DEBUG) << "Debug offsets were never found";
        return 0;
    }

    LOG(INFO) << "Searching for PyInterpreterState based on PyRuntime address " << std::hex
              << std::showbase << d_debug_offsets_addr
              << " found when searching for 3.13+ debug offsets";

    try {
        Structure<py_runtime_v> runtime(shared_from_this(), d_debug_offsets_addr);
        remote_addr_t interp_state = runtime.getField(&py_runtime_v::o_interp_head);
        LOG(DEBUG) << "Checking interpreter state at " << std::hex << std::showbase << interp_state
                   << " found at address "
                   << runtime.getFieldRemoteAddress(&py_runtime_v::o_interp_head);
        if (isValidInterpreterState(interp_state)) {
            LOG(DEBUG) << "Interpreter head reference from debug offsets dereferences successfully";
            return interp_state;
        }
    } catch (...) {
        // Swallow exceptions and fall through to return failure
    }
    LOG(INFO) << "Failed to resolve PyInterpreterState based on PyRuntime address " << std::hex
              << std::showbase << d_debug_offsets_addr;
    return 0;
}

ProcessManager::ProcessManager(
        pid_t pid,
        const std::shared_ptr<ProcessTracer>& tracer,
        const std::shared_ptr<ProcessAnalyzer>& analyzer,
        std::vector<VirtualMap> memory_maps,
        MemoryMapInformation map_info)
: AbstractProcessManager(pid, std::move(memory_maps), std::move(map_info))
, tracer(tracer)
{
    if (tracer) {
        d_tids = tracer->getTids();
    } else {
        d_tids = getProcessTids(pid);
    }
    d_manager = std::make_unique<ProcessMemoryManager>(pid, d_memory_maps);
    d_analyzer = analyzer;
    d_unwinder = std::make_unique<Unwinder>(analyzer);
}

const std::vector<int>&
ProcessManager::Tids() const
{
    return d_tids;
}

CoreFileProcessManager::CoreFileProcessManager(
        pid_t pid,
        const std::shared_ptr<CoreFileAnalyzer>& analyzer,
        std::vector<VirtualMap> memory_maps,
        MemoryMapInformation map_info)
: AbstractProcessManager(pid, std::move(memory_maps), std::move(map_info))
{
    d_analyzer = analyzer;
    d_manager = std::make_unique<CorefileRemoteMemoryManager>(analyzer, d_memory_maps);
    d_executable = analyzer->d_executable;
    std::unique_ptr<CoreFileUnwinder> the_unwinder = std::make_unique<CoreFileUnwinder>(analyzer);
    d_tids = the_unwinder->getCoreTids();
    d_unwinder = std::move(the_unwinder);
}

const std::vector<int>&
CoreFileProcessManager::Tids() const
{
    return d_tids;
}

}  // namespace pystack
