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

    PyInterpreterState is;
    // The check for valid addresses may fail if the address falls in the stack
    // space (there are "holes" in the address map space so just checking for
    // min_addr < addr < max_addr does not guarantee a valid address) so we need
    // to catch InvalidRemoteAddress exceptions.
    try {
        copyObjectFromProcess(addr, &is);
    } catch (RemoteMemCopyError& ex) {
        return false;
    }

    PyThreadState current_thread;
    auto current_thread_addr = getField(is, &py_is_v::o_tstate_head);
    if (!isAddressValid(current_thread_addr)) {
        return false;
    }

    try {
        copyObjectFromProcess(current_thread_addr, &current_thread);
    } catch (RemoteMemCopyError& ex) {
        return false;
    }

    if (getField(current_thread, &py_thread_v::o_interp) != addr) {
        return false;
    }

    LOG(DEBUG) << std::hex << std::showbase << "Possible PyInterpreterState candidate at address "
               << addr << " with tstate_head value of " << current_thread_addr;

    // Validate dictionaries in the interpreter state
    std::unordered_map<std::string, remote_addr_t> dictionaries(
            {{"modules", getField(is, &py_is_v::o_modules)},
             {"sysdict", getField(is, &py_is_v::o_sysdict)},
             {"builtins", getField(is, &py_is_v::o_builtins)}});
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

    PyRuntimeState py_runtime;
    copyObjectFromProcess(runtime_addr, &py_runtime);
    remote_addr_t interp_state = getField(py_runtime, &py_runtime_v::o_interp_head);

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
    Python3::PyUnicodeObject unicode;
    std::vector<char> buffer;
    ssize_t len;
    remote_addr_t data_addr;

    if (d_major == 2) {
        LOG(DEBUG) << std::hex << std::showbase << "Handling unicode object of version 2 from address "
                   << addr;
        copyObjectFromProcess(addr, &string);

        len = string.ob_base.ob_size;
        buffer.resize(len);
        data_addr = (remote_addr_t)((char*)addr + offsetof(Python2::_PyStringObject, ob_sval));
        LOG(DEBUG) << std::hex << std::showbase << "Copying ASCII data for unicode object from address "
                   << data_addr;
        copyMemoryFromProcess(data_addr, len, buffer.data());
    } else {
        LOG(DEBUG) << std::hex << std::showbase << "Handling unicode object of version 3 from address "
                   << addr;
        copyObjectFromProcess(addr, &unicode);
        if (unicode._base._base.state.kind != 1u) {
            throw InvalidRemoteObject();
        }
        if (unicode._base._base.state.compact != 1u) {
            throw InvalidRemoteObject();
        }
        len = unicode._base._base.length;
        buffer.resize(len);

        size_t offset;
        if (d_major > 3 || (d_major == 3 && d_minor >= 12)) {
            offset = sizeof(Python3_12::PyASCIIObject);
        } else {
            offset = sizeof(Python3::PyASCIIObject);
        }

        data_addr = ((remote_addr_t)((char*)addr + offset));
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
        PyBytesObject bytes;

        copyObjectFromProcess(addr, &bytes);

        if ((len = bytes.ob_base.ob_size + 1) < 1) {
            throw std::runtime_error("Incorrect size of the fetches bytes object");
        }
        buffer.resize(len);
        data_addr = (remote_addr_t)((char*)addr + offsetof(PyBytesObject, ob_sval));
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
        PyRuntimeState py_runtime;
        copyObjectFromProcess(runtime_addr, &py_runtime);
        remote_addr_t p = getField(py_runtime, &py_runtime_v::o_finalizing);
        return p == 0 ? InterpreterStatus::RUNNING : InterpreterStatus::FINALIZED;
    }

    return InterpreterStatus::UNKNOWN;
}

std::pair<int, int>
AbstractProcessManager::findPythonVersion() const
{
    auto version_symbol = findSymbol("Py_Version");
    if (!version_symbol) {
        LOG(DEBUG) << "Faled to determine Python version from symbols";
        return {-1, -1};
    }
    unsigned long version;
    copyObjectFromProcess(version_symbol, &version);
    int major = (version >> 24) & 0xFF;
    int minor = (version >> 16) & 0xFF;
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

bool
AbstractProcessManager::versionIsAtLeast(int required_major, int required_minor) const
{
    return d_major > required_major || (d_major == required_major && d_minor >= required_minor);
}

const python_v&
AbstractProcessManager::offsets() const
{
    return *d_py_v;
}

remote_addr_t
AbstractProcessManager::findInterpreterStateFromElfData() const
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
    return findInterpreterStateFromPyRuntime(load_addr + section_info.corrected_addr);
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
