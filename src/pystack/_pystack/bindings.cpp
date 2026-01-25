#include <nanobind/nanobind.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/vector.h>

#include <filesystem>
#include <memory>
#include <optional>

#include "corefile.h"
#include "elf_common.h"
#include "logging.h"
#include "maps_parser.h"
#include "mem.h"
#include "process.h"
#include "thread_builder.h"

namespace nb = nanobind;
using namespace nb::literals;
// Note: We don't use "using namespace pystack;" because it conflicts with Python's PyObject

// Simple exception classes that store the message
class NotEnoughInformationError : public std::exception
{
  public:
    explicit NotEnoughInformationError(const std::string& message)
    : d_message(message)
    {
    }
    const char* what() const noexcept override
    {
        return d_message.c_str();
    }

  private:
    std::string d_message;
};

class EngineError : public std::exception
{
  public:
    explicit EngineError(const std::string& message)
    : d_message(message)
    {
    }
    const char* what() const noexcept override
    {
        return d_message.c_str();
    }

  private:
    std::string d_message;
};

[[noreturn]] void
raise_not_enough_information(const char* message)
{
    throw NotEnoughInformationError(message);
}

// StackMethod enum values (must match Python enum)
enum class StackMethod {
    ELF_DATA = 1 << 0,
    SYMBOLS = 1 << 1,
    BSS = 1 << 2,
    ANONYMOUS_MAPS = 1 << 3,
    HEAP = 1 << 4,
    DEBUG_OFFSETS = 1 << 5,
    AUTO = (1 << 5) | (1 << 0) | (1 << 1) | (1 << 2),
    ALL = (1 << 5) | (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4),
};

enum class NativeReportingMode {
    OFF = 0,
    PYTHON = 1,
    ALL = 1000,
    LAST = 2000,
};

class CoreFileAnalyzerWrapper
{
  public:
    CoreFileAnalyzerWrapper(
            const std::filesystem::path& corefile,
            std::optional<std::filesystem::path> executable = std::nullopt,
            std::optional<std::filesystem::path> lib_search_path = std::nullopt)
    : d_ignored_libs({"ld-linux", "linux-vdso"})
    {
        std::string corefile_str = corefile.string();
        if (executable && lib_search_path) {
            d_analyzer = std::make_shared<pystack::CoreFileAnalyzer>(
                    corefile_str,
                    executable->string(),
                    lib_search_path->string());
        } else if (executable) {
            d_analyzer = std::make_shared<pystack::CoreFileAnalyzer>(corefile_str, executable->string());
        } else {
            d_analyzer = std::make_shared<pystack::CoreFileAnalyzer>(corefile_str);
        }
        d_extractor = std::make_unique<pystack::CoreFileExtractor>(d_analyzer);
    }

    nb::list extract_maps() const
    {
        auto mapped_files = d_extractor->extractMappedFiles();
        auto memory_maps = d_extractor->MemoryMaps();
        auto maps = parseCoreFileMaps(mapped_files, memory_maps);

        nb::module_ pystack_maps = nb::module_::import_("pystack.maps");
        nb::object VirtualMap = pystack_maps.attr("VirtualMap");

        nb::list result;
        for (const auto& map : maps) {
            std::string path_str = map.Path();
            nb::object path_obj =
                    path_str.empty() ? nb::none() : nb::cast(std::filesystem::path(path_str));
            nb::object vm = VirtualMap(
                    map.Start(),
                    map.End(),
                    map.FileSize(),
                    map.Offset(),
                    map.Device(),
                    map.Flags(),
                    map.Inode(),
                    path_obj);
            result.append(vm);
        }
        return result;
    }

    int extract_pid() const
    {
        return d_extractor->Pid();
    }

    std::filesystem::path extract_executable() const
    {
        return std::filesystem::path(d_extractor->extractExecutable());
    }

    nb::dict extract_failure_info() const
    {
        auto info = d_extractor->extractFailureInfo();
        nb::dict result;
        result["si_signo"] = info.si_signo;
        result["si_errno"] = info.si_errno;
        result["si_code"] = info.si_code;
        result["sender_pid"] = info.sender_pid;
        result["sender_uid"] = info.sender_uid;
        result["failed_addr"] = info.failed_addr;
        return result;
    }

    nb::dict extract_ps_info() const
    {
        auto info = d_extractor->extractPSInfo();
        nb::dict result;
        result["state"] = static_cast<int>(info.state);
        result["sname"] = static_cast<int>(info.sname);
        result["zomb"] = static_cast<int>(info.zomb);
        result["nice"] = static_cast<int>(info.nice);
        result["flag"] = info.flag;
        result["uid"] = info.uid;
        result["gid"] = info.gid;
        result["pid"] = info.pid;
        result["ppid"] = info.ppid;
        result["pgrp"] = info.pgrp;
        result["sid"] = info.sid;
        result["fname"] = std::string(info.fname);
        result["psargs"] = std::string(info.psargs);
        return result;
    }

    std::vector<std::string> missing_modules() const
    {
        std::vector<std::string> result;
        for (const auto& mod : d_extractor->missingModules()) {
            if (!isIgnoredLib(mod)) {
                result.push_back(mod);
            }
        }
        for (const auto& memmap : d_extractor->MemoryMaps()) {
            std::string path = memmap.path;
            if (path.empty() || isIgnoredLib(path)) {
                continue;
            }
            // Check if path exists
            std::ifstream f(path);
            if (f.good()) {
                continue;
            }
            // Check if already in result
            auto fname = std::filesystem::path(path).filename().string();
            bool found = false;
            for (const auto& r : result) {
                if (std::filesystem::path(r).filename().string() == fname) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                result.push_back(path);
            }
        }
        return result;
    }

    nb::dict extract_module_load_points() const
    {
        nb::dict result;
        for (const auto& mod : d_extractor->ModuleInformation()) {
            auto name = std::filesystem::path(mod.filename).filename().string();
            result[nb::cast(name)] = mod.start;
        }
        return result;
    }

    nb::list extract_build_ids() const
    {
        nb::list result;
        auto memory_maps = d_extractor->MemoryMaps();
        auto module_info = d_extractor->ModuleInformation();

        std::unordered_map<std::string, std::string> maps_by_file;
        for (const auto& map : memory_maps) {
            maps_by_file[map.path] = map.buildid;
        }

        for (const auto& mod : module_info) {
            if (isIgnoredLib(mod.filename)) {
                continue;
            }
            auto map_buildid_it = maps_by_file.find(mod.filename);
            std::string map_buildid =
                    (map_buildid_it != maps_by_file.end()) ? map_buildid_it->second : "";
            result.append(nb::make_tuple(mod.filename, mod.buildid, map_buildid));
        }
        return result;
    }

  private:
    bool isIgnoredLib(const std::string& path) const
    {
        for (const auto& prefix : d_ignored_libs) {
            if (path.find(prefix) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    std::shared_ptr<pystack::CoreFileAnalyzer> d_analyzer;
    std::unique_ptr<pystack::CoreFileExtractor> d_extractor;
    std::vector<std::string> d_ignored_libs;
};

class ProcessManagerWrapper
{
  public:
    explicit ProcessManagerWrapper(std::shared_ptr<pystack::AbstractProcessManager> manager)
    : d_manager(std::move(manager))
    {
    }

    static std::unique_ptr<ProcessManagerWrapper> create_from_pid(pid_t pid, bool stop_process)
    {
        auto manager = pystack::ProcessManager::create(pid, stop_process);
        return std::make_unique<ProcessManagerWrapper>(std::move(manager));
    }

    static std::unique_ptr<ProcessManagerWrapper> create_from_core(
            const std::filesystem::path& core_file,
            const std::filesystem::path& executable,
            std::optional<std::filesystem::path> lib_search_path)
    {
        std::optional<std::string> lib_path_str;
        if (lib_search_path) {
            lib_path_str = lib_search_path->string();
        }
        auto manager = pystack::CoreFileProcessManager::create(
                core_file.string(),
                executable.string(),
                lib_path_str);
        return std::make_unique<ProcessManagerWrapper>(std::move(manager));
    }

    int interpreter_status() const
    {
        return static_cast<int>(d_manager->isInterpreterActive());
    }

    bool is_interpreter_active() const
    {
        return d_manager->isInterpreterActive()
               == pystack::AbstractProcessManager::InterpreterStatus::RUNNING;
    }

    void reset()
    {
        d_manager.reset();
    }

    pid_t pid() const
    {
        return d_manager->Pid();
    }

    std::pair<int, int> python_version() const
    {
        return d_manager->Version();
    }

    const std::vector<pystack::VirtualMap>& virtual_maps() const
    {
        return d_manager->MemoryMaps();
    }

    std::shared_ptr<pystack::AbstractProcessManager> get_manager() const
    {
        return d_manager;
    }

  private:
    std::shared_ptr<pystack::AbstractProcessManager> d_manager;
};

nb::bytes
copy_memory_from_address(pid_t pid, uintptr_t address, size_t size)
{
    auto manager = std::make_shared<pystack::ProcessMemoryManager>(pid);
    PyObject* py_bytes = PyBytes_FromStringAndSize(nullptr, static_cast<Py_ssize_t>(size));
    if (!py_bytes) {
        throw nb::python_error();
    }
    manager->copyMemoryFromProcess(address, size, PyBytes_AS_STRING(py_bytes));
    return nb::steal<nb::bytes>(py_bytes);
}

nb::object
get_bss_info(const std::filesystem::path& binary)
{
    pystack::SectionInfo info;
    if (pystack::getSectionInfo(binary.string(), ".bss", &info)) {
        nb::dict result;
        result["name"] = info.name;
        result["flags"] = info.flags;
        result["addr"] = info.addr;
        result["corrected_addr"] = info.corrected_addr;
        result["offset"] = info.offset;
        result["size"] = info.size;
        return result;
    }
    return nb::none();
}

// Helper struct to hold Python type objects for thread building
struct PyTypes
{
    nb::object PyThread;
    nb::object PyFrame;
    nb::object PyCodeObject;
    nb::object LocationInfo;
    nb::object NativeFrame;

    static PyTypes load()
    {
        nb::module_ pystack_types = nb::module_::import_("pystack.types");
        return {pystack_types.attr("PyThread"),
                pystack_types.attr("PyFrame"),
                pystack_types.attr("PyCodeObject"),
                pystack_types.attr("LocationInfo"),
                pystack_types.attr("NativeFrame")};
    }
};

// Build frame chain from C++ thread data
nb::object
buildFrameChain(const pystack::PyThreadData& thread, const PyTypes& types)
{
    nb::object first_frame = nb::none();
    nb::object prev_frame = nb::none();

    // Frames from C++ are in innermost-to-outermost order
    // Python iterates via .next and expects: <module> -> first_func -> second_func -> third_func
    // So we iterate in reverse to build the list in the correct order
    for (auto it = thread.frames.rbegin(); it != thread.frames.rend(); ++it) {
        const auto& frame_data = *it;
        nb::object location = types.LocationInfo(
                frame_data.code.location.lineno,
                frame_data.code.location.end_lineno,
                frame_data.code.location.column,
                frame_data.code.location.end_column);
        nb::object code = types.PyCodeObject(frame_data.code.filename, frame_data.code.scope, location);

        nb::dict args;
        for (const auto& [k, v] : frame_data.arguments) {
            args[nb::cast(k)] = v;
        }
        nb::dict locs;
        for (const auto& [k, v] : frame_data.locals) {
            locs[nb::cast(k)] = v;
        }

        nb::object py_frame = types.PyFrame(
                prev_frame,
                nb::none(),
                code,
                args,
                locs,
                frame_data.is_entry,
                frame_data.is_shim);

        if (!prev_frame.is_none()) {
            prev_frame.attr("next") = py_frame;
        }

        if (first_frame.is_none()) {
            first_frame = py_frame;
        }
        prev_frame = py_frame;
    }

    return first_frame;
}

// Build native frames list
nb::list
buildNativeFramesList(const std::vector<pystack::NativeFrame>& native_frames, const PyTypes& types)
{
    nb::list result;
    for (const auto& nf : native_frames) {
        result.append(types.NativeFrame(
                nf.address,
                nf.symbol,
                nf.path,
                nf.linenumber,
                nf.colnumber,
                nf.library));
    }
    return result;
}

// Build a Python thread object from C++ thread data
nb::object
buildPyThreadObject(
        const pystack::PyThreadData& thread,
        const PyTypes& types,
        std::pair<int, int> python_version)
{
    nb::object first_frame = buildFrameChain(thread, types);
    nb::list native_frames = buildNativeFramesList(thread.native_frames, types);

    return types.PyThread(
            thread.tid,
            first_frame,
            native_frames,
            thread.gil_status,
            thread.gc_status,
            nb::make_tuple(python_version.first, python_version.second),
            "name"_a = thread.name ? nb::cast(*thread.name) : nb::none());
}

// Build a native-only thread object (no Python frames)
nb::object
buildNativeOnlyThreadObject(const pystack::PyThreadData& thread, const PyTypes& types)
{
    nb::list native_frames = buildNativeFramesList(thread.native_frames, types);

    return types.PyThread(
            thread.tid,
            nb::none(),
            native_frames,
            0,
            0,
            nb::none(),
            "name"_a = thread.name ? nb::cast(*thread.name) : nb::none());
}

// Log interpreter status
void
logInterpreterStatus(int status)
{
    if (status == static_cast<int>(pystack::AbstractProcessManager::InterpreterStatus::FINALIZED)) {
        pystack::LOG(pystack::WARNING)
                << "The interpreter is shutting itself down so it is possible that no "
                   "Python stack trace is available for inspection.";
    } else if (status == static_cast<int>(pystack::AbstractProcessManager::InterpreterStatus::RUNNING)) {
        pystack::LOG(pystack::INFO) << "An active interpreter has been detected";
    }
}

// Log available memory maps
void
logMemoryMaps(const std::vector<pystack::VirtualMap>& maps, const char* source)
{
    pystack::LOG(pystack::DEBUG) << "Available memory maps for " << source << ":";
    for (const auto& map : maps) {
        pystack::LOG(pystack::DEBUG)
                << "  " << std::hex << map.Start() << "-" << map.End() << " " << map.Path();
    }
}

nb::object
get_process_threads(
        pid_t pid,
        bool stop_process,
        NativeReportingMode native_mode,
        bool locals,
        StackMethod method)
{
    auto types = PyTypes::load();

    try {
        // Collect all C++ data with GIL released so other threads can run
        // (e.g. concurrent ptrace attachment attempts will see EPERM).
        std::vector<pystack::PyThreadData> python_threads;
        std::vector<pystack::PyThreadData> native_only_threads;
        std::pair<int, int> python_version;
        bool not_enough_info = false;

        {
            nb::gil_scoped_release release;

            auto manager = ProcessManagerWrapper::create_from_pid(pid, stop_process);
            logMemoryMaps(manager->virtual_maps(), "process");

            if (native_mode != NativeReportingMode::ALL) {
                logInterpreterStatus(manager->interpreter_status());
            }

            pystack::remote_addr_t head = pystack::getInterpreterStateAddr(
                    manager->get_manager().get(),
                    static_cast<int>(method));

            if (head == 0 && native_mode != NativeReportingMode::ALL) {
                not_enough_info = true;
            } else {
                python_version = manager->python_version();
                std::vector<int> all_tids = pystack::getThreadIds(manager->get_manager());

                if (head != 0) {
                    bool add_native = native_mode != NativeReportingMode::OFF;
                    python_threads = pystack::buildThreadsFromInterpreter(
                            manager->get_manager(),
                            head,
                            pid,
                            add_native,
                            locals);

                    for (const auto& thread : python_threads) {
                        all_tids.erase(
                                std::remove(all_tids.begin(), all_tids.end(), thread.tid),
                                all_tids.end());
                    }
                }

                if (native_mode == NativeReportingMode::ALL) {
                    for (int tid : all_tids) {
                        native_only_threads.push_back(
                                pystack::buildNativeThread(manager->get_manager(), pid, tid));
                    }
                }

                manager->reset();
            }
        }

        // GIL re-acquired: build Python objects
        if (not_enough_info) {
            raise_not_enough_information(
                    "Could not gather enough information to extract the Python frame information");
        }

        nb::list result;
        for (const auto& thread : python_threads) {
            result.append(buildPyThreadObject(thread, types, python_version));
        }
        for (const auto& thread : native_only_threads) {
            result.append(buildNativeOnlyThreadObject(thread, types));
        }
        return result;
    } catch (const NotEnoughInformationError&) {
        throw;
    } catch (const EngineError&) {
        throw;
    } catch (const std::exception& e) {
        throw EngineError(e.what());
    }
}

nb::object
get_process_threads_for_core(
        const std::filesystem::path& core_file,
        const std::filesystem::path& executable,
        std::optional<std::filesystem::path> library_search_path,
        NativeReportingMode native_mode,
        bool locals,
        StackMethod method)
{
    auto types = PyTypes::load();

    try {
        auto manager =
                ProcessManagerWrapper::create_from_core(core_file, executable, library_search_path);
        logMemoryMaps(manager->virtual_maps(), "core");

        if (native_mode != NativeReportingMode::ALL) {
            logInterpreterStatus(manager->interpreter_status());
        }

        pystack::remote_addr_t head =
                pystack::getInterpreterStateAddr(manager->get_manager().get(), static_cast<int>(method));

        if (head == 0 && native_mode != NativeReportingMode::ALL) {
            raise_not_enough_information(
                    "Could not gather enough information to extract the Python frame information");
        }

        nb::list result;
        std::vector<int> all_tids = pystack::getThreadIds(manager->get_manager());

        if (head != 0) {
            bool add_native = native_mode == NativeReportingMode::PYTHON
                              || native_mode == NativeReportingMode::ALL;
            auto threads = pystack::buildThreadsFromInterpreter(
                    manager->get_manager(),
                    head,
                    manager->pid(),
                    add_native,
                    locals);

            for (const auto& thread : threads) {
                result.append(buildPyThreadObject(thread, types, manager->python_version()));
                all_tids.erase(
                        std::remove(all_tids.begin(), all_tids.end(), thread.tid),
                        all_tids.end());
            }
        }

        if (native_mode == NativeReportingMode::ALL) {
            for (int tid : all_tids) {
                auto thread = pystack::buildNativeThread(manager->get_manager(), manager->pid(), tid);
                result.append(buildNativeOnlyThreadObject(thread, types));
            }
        }

        return result;
    } catch (const NotEnoughInformationError&) {
        throw;
    } catch (const EngineError&) {
        throw;
    } catch (const std::exception& e) {
        throw EngineError(e.what());
    }
}

void
_check_interpreter_shutdown(nb::object manager)
{
    int status = nb::cast<int>(manager.attr("interpreter_status")());

    if (status == static_cast<int>(pystack::AbstractProcessManager::InterpreterStatus::FINALIZED)) {
        pystack::LOG(pystack::WARNING)
                << "The interpreter is shutting itself down so it is possible that no "
                   "Python stack trace is available for inspection.";
    } else if (status != -1) {
        // -1 means failed to detect, 2 means FINALIZED (already handled above)
        // Other values mean running/active
        pystack::LOG(pystack::INFO) << "An active interpreter has been detected";
    }
}

NB_MODULE(_pystack, m)
{
    m.doc() = "PyStack native extension module";

    nb::register_exception_translator([](const std::exception_ptr& p, void*) {
        try {
            if (p) std::rethrow_exception(p);
        } catch (const NotEnoughInformationError& e) {
            nb::object exc_type = nb::module_::import_("pystack.errors").attr("NotEnoughInformation");
            PyErr_SetString(exc_type.ptr(), e.what());
        } catch (const EngineError& e) {
            nb::object exc_type = nb::module_::import_("pystack.errors").attr("EngineError");
            PyErr_SetString(exc_type.ptr(), e.what());
        }
    });

    pystack::initializePythonLoggerInterface();

    nb::enum_<StackMethod>(m, "StackMethod", nb::is_flag())
            .value("ELF_DATA", StackMethod::ELF_DATA)
            .value("SYMBOLS", StackMethod::SYMBOLS)
            .value("BSS", StackMethod::BSS)
            .value("ANONYMOUS_MAPS", StackMethod::ANONYMOUS_MAPS)
            .value("HEAP", StackMethod::HEAP)
            .value("DEBUG_OFFSETS", StackMethod::DEBUG_OFFSETS)
            .value("AUTO", StackMethod::AUTO)
            .value("ALL", StackMethod::ALL);

    nb::enum_<NativeReportingMode>(m, "NativeReportingMode")
            .value("OFF", NativeReportingMode::OFF)
            .value("PYTHON", NativeReportingMode::PYTHON)
            .value("ALL", NativeReportingMode::ALL)
            .value("LAST", NativeReportingMode::LAST);

    nb::class_<CoreFileAnalyzerWrapper>(m, "CoreFileAnalyzer")
            .def(nb::init<
                         const std::filesystem::path&,
                         std::optional<std::filesystem::path>,
                         std::optional<std::filesystem::path>>(),
                 "core_file"_a,
                 "executable"_a = nb::none(),
                 "lib_search_path"_a = nb::none())
            .def("extract_maps", &CoreFileAnalyzerWrapper::extract_maps)
            .def("extract_pid", &CoreFileAnalyzerWrapper::extract_pid)
            .def("extract_executable", &CoreFileAnalyzerWrapper::extract_executable)
            .def("extract_failure_info", &CoreFileAnalyzerWrapper::extract_failure_info)
            .def("extract_ps_info", &CoreFileAnalyzerWrapper::extract_ps_info)
            .def("missing_modules", &CoreFileAnalyzerWrapper::missing_modules)
            .def("extract_module_load_points", &CoreFileAnalyzerWrapper::extract_module_load_points)
            .def("extract_build_ids", &CoreFileAnalyzerWrapper::extract_build_ids);

    nb::class_<ProcessManagerWrapper>(m, "ProcessManager")
            .def_static(
                    "create_from_pid",
                    &ProcessManagerWrapper::create_from_pid,
                    "pid"_a,
                    "stop_process"_a = true)
            .def_static(
                    "create_from_core",
                    &ProcessManagerWrapper::create_from_core,
                    "core_file"_a,
                    "executable"_a,
                    "lib_search_path"_a = nb::none())
            .def("interpreter_status", &ProcessManagerWrapper::interpreter_status)
            .def("is_interpreter_active", &ProcessManagerWrapper::is_interpreter_active)
            .def_prop_ro("pid", &ProcessManagerWrapper::pid)
            .def_prop_ro("python_version", &ProcessManagerWrapper::python_version)
            .def(
                    "__enter__",
                    [](ProcessManagerWrapper& self) -> ProcessManagerWrapper& { return self; },
                    nb::rv_policy::reference)
            .def("__exit__", [](ProcessManagerWrapper& self, nb::args) { self.reset(); });

    m.def("copy_memory_from_address",
          &copy_memory_from_address,
          "pid"_a,
          "address"_a,
          "size"_a,
          "Copy memory from a remote process");

    m.def("get_bss_info", &get_bss_info, "binary"_a, "Get BSS section information from an ELF binary");

    // Note: We use nb::arg().none() to allow None to be passed explicitly
    m.def(
            "get_process_threads",
            [](pid_t pid,
               bool stop_process,
               NativeReportingMode native_mode,
               bool locals,
               nb::object method_obj) {
                if (method_obj.is_none()) {
                    throw std::invalid_argument("Invalid method for stack analysis");
                }
                StackMethod method = nb::cast<StackMethod>(method_obj);
                return get_process_threads(pid, stop_process, native_mode, locals, method);
            },
            "pid"_a,
            "stop_process"_a = true,
            "native_mode"_a = NativeReportingMode::OFF,
            "locals"_a = false,
            nb::arg("method").none() = nb::cast(StackMethod::AUTO),
            "Return an iterable of Thread objects from a live process");

    m.def(
            "get_process_threads_for_core",
            [](const std::filesystem::path& core_file,
               const std::filesystem::path& executable,
               std::optional<std::filesystem::path> library_search_path,
               NativeReportingMode native_mode,
               bool locals,
               nb::object method_obj) {
                if (method_obj.is_none()) {
                    throw std::invalid_argument("Invalid method for stack analysis");
                }
                StackMethod method = nb::cast<StackMethod>(method_obj);
                return get_process_threads_for_core(
                        core_file,
                        executable,
                        library_search_path,
                        native_mode,
                        locals,
                        method);
            },
            "core_file"_a,
            "executable"_a,
            "library_search_path"_a = nb::none(),
            "native_mode"_a = NativeReportingMode::PYTHON,
            "locals"_a = false,
            nb::arg("method").none() = nb::cast(StackMethod::AUTO),
            "Return an iterable of Thread objects from a core file");

    m.def("_check_interpreter_shutdown",
          &_check_interpreter_shutdown,
          "manager"_a,
          "Check interpreter shutdown status and log appropriately");

    // intercept_runtime_errors decorator - re-export from pystack.errors
    nb::module_ pystack_errors = nb::module_::import_("pystack.errors");
    m.attr("intercept_runtime_errors") = pystack_errors.attr("intercept_runtime_errors");
}
