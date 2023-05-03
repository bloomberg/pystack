import contextlib
import enum
import functools
import logging
import os
import pathlib
from typing import Any
from typing import Callable
from typing import Dict
from typing import Iterable
from typing import List
from typing import Optional
from typing import Tuple
from typing import TypeVar

from cython.operator import dereference
from cython.operator import postincrement

from _pystack.corefile cimport CoreFileExtractor
from _pystack.elf_common cimport CoreFileAnalyzer as NativeCoreFileAnalyzer
from _pystack.elf_common cimport ProcessAnalyzer as NativeProcessAnalyzer
from _pystack.elf_common cimport SectionInfo
from _pystack.elf_common cimport getSectionInfo
from _pystack.logging cimport initializePythonLoggerInterface
from _pystack.mem cimport AbstractRemoteMemoryManager
from _pystack.mem cimport BlockingProcessMemoryManager
from _pystack.mem cimport MemoryMapInformation as CppMemoryMapInformation
from _pystack.mem cimport ProcessMemoryManager
from _pystack.mem cimport VirtualMap as CppVirtualMap
from _pystack.process cimport AbstractProcessManager
from _pystack.process cimport CoreFileProcessManager
from _pystack.process cimport InterpreterStatus
from _pystack.process cimport ProcessManager as NativeProcessManager
from _pystack.process cimport remote_addr_t
from _pystack.pycode cimport CodeObject
from _pystack.pyframe cimport FrameObject
from _pystack.pythread cimport NativeThread
from _pystack.pythread cimport Thread
from _pystack.pythread cimport getThreadFromInterpreterState
from cpython.unicode cimport PyUnicode_Decode
from libcpp.memory cimport make_shared
from libcpp.memory cimport make_unique
from libcpp.memory cimport shared_ptr
from libcpp.memory cimport unique_ptr
from libcpp.string cimport string as cppstring
from libcpp.unordered_map cimport unordered_map
from libcpp.vector cimport vector

from .errors import CoreExecutableNotFound
from .errors import EngineError
from .errors import InvalidPythonProcess
from .errors import NotEnoughInformation
from .maps import MemoryMapInformation
from .maps import VirtualMap
from .maps import generate_maps_for_process
from .maps import generate_maps_from_core_data
from .maps import parse_maps_file
from .maps import parse_maps_file_for_binary
from .process import get_python_version_for_core
from .process import get_python_version_for_process
from .process import get_thread_name
from .types import LocationInfo
from .types import NativeFrame
from .types import PyCodeObject
from .types import PyFrame
from .types import PyThread

LOGGER = logging.getLogger(__file__)

initializePythonLoggerInterface()


class StackMethod(enum.Enum):
    ELF_DATA = 1 << 0
    SYMBOLS = 1 << 1
    BSS = 1 << 2
    ANONYMOUS_MAPS = 1 << 3
    HEAP = 1 << 4
    AUTO = ELF_DATA | SYMBOLS | BSS
    ALL = AUTO | ANONYMOUS_MAPS | HEAP


class NativeReportingMode(enum.Enum):
    OFF = 0
    PYTHON = 1
    ALL = 1000


cdef api void log_with_python(const char* message, int level):
    with contextlib.suppress(UnicodeDecodeError):
        LOGGER.log(level, message)

T = TypeVar("T", bound=Callable[..., Any])


class intercept_runtime_errors:
    def __init__(self, exception=EngineError):
        self.exception = exception

    def __call__(self, func: T) -> T:
        @functools.wraps(func)
        def wrapper(*args: Any, **kwargs: Any) -> Any:
            try:
                return func(*args, **kwargs)
            except RuntimeError as e:
                raise self.exception(*e.args) from e

        return wrapper


@intercept_runtime_errors(EngineError)
def copy_memory_from_address(pid, address, size):
    cdef shared_ptr[AbstractRemoteMemoryManager] manager
    cdef int the_pid = pid
    cdef vector[int] tids
    manager = <shared_ptr[AbstractRemoteMemoryManager]> (
        make_shared[ProcessMemoryManager](the_pid)
    )

    cdef AbstractRemoteMemoryManager *manager_handle = manager.get()

    memory = bytearray(size)
    cdef char *buffer = memory
    cdef remote_addr_t _address = address
    manager_handle.copyMemoryFromProcess(_address, size, <void *> buffer)
    manager.reset()
    return memory


cdef CppVirtualMap _pymap_to_map(pymap: VirtualMap) except *:
    default_path = ""
    assert pymap is not None
    return CppVirtualMap(
        pymap.start,
        pymap.end,
        pymap.filesize,
        pymap.flags,
        pymap.offset,
        pymap.device,
        pymap.inode,
        str(pymap.path) if pymap.path else default_path,
    )


cdef CppMemoryMapInformation _pymapinfo_to_mapinfo(map_info: MemoryMapInformation):
    interpreter_map = (
        map_info.libpython if map_info.libpython is not None else map_info.python
    )
    cdef CppMemoryMapInformation cppmap_info
    assert(interpreter_map is not None)
    cppmap_info.setMainMap(_pymap_to_map(interpreter_map))
    if map_info.bss:
        cppmap_info.setBss(_pymap_to_map(map_info.bss))
    if map_info.heap:
        cppmap_info.setHeap(_pymap_to_map(map_info.heap))

    return cppmap_info


cdef vector[CppVirtualMap] _pymaps_to_maps(pymaps: Iterable[VirtualMap]) except *:
    cdef vector[CppVirtualMap] native_maps
    for pymap in pymaps:
        native_maps.push_back(_pymap_to_map(pymap))
    return native_maps


def get_bss_info(binary):
    cdef SectionInfo _result
    if getSectionInfo(str(binary), b".bss", &_result):
        result = _result
        return result
    return None

######################
# MANAGEMENT CLASSES #
######################

cdef shared_ptr[NativeCoreFileAnalyzer] get_core_analyzer(
    core_file, executable=None, lib_search_path=None
) except *:
    cdef shared_ptr[NativeCoreFileAnalyzer] analyzer;
    cdef cppstring the_core_file, the_executable, the_lib_search_path
    the_core_file = str(core_file)
    if executable is not None and lib_search_path is not None:
        the_executable = str(executable)
        the_lib_search_path = str(lib_search_path)
        analyzer = make_shared[NativeCoreFileAnalyzer](
            the_core_file, the_executable, the_lib_search_path
        )
    elif executable is not None and lib_search_path is None:
        the_executable = str(executable)
        analyzer = make_shared[NativeCoreFileAnalyzer](the_core_file, the_executable)
    else:
        analyzer = make_shared[NativeCoreFileAnalyzer](the_core_file)
    return analyzer


cdef class CoreFileAnalyzer:
    cdef shared_ptr[CoreFileExtractor] _core_analyzer
    cdef object ignored_libs

    def __cinit__(self, core_file, executable=None, lib_search_path=None):
        self.ignored_libs = frozenset(("ld-linux", "linux-vdso"))
        self._initialize_core_analyzer(core_file, executable, lib_search_path)

    @intercept_runtime_errors(EngineError)
    def _initialize_core_analyzer(self, core_file, executable, lib_search_path) -> None:
        cdef shared_ptr[NativeCoreFileAnalyzer] analyzer = get_core_analyzer(
            core_file, executable, lib_search_path
        )
        self._core_analyzer = make_shared[CoreFileExtractor](analyzer)

    @intercept_runtime_errors(EngineError)
    def extract_maps(self) -> Iterable[Dict[str, Any]]:
        mapped_files = self._core_analyzer.get().extractMappedFiles()
        memory_maps = self._core_analyzer.get().MemoryMaps()
        return generate_maps_from_core_data(mapped_files, memory_maps)

    @intercept_runtime_errors(EngineError)
    def extract_pid(self) -> int:
        return self._core_analyzer.get().Pid()

    @intercept_runtime_errors(CoreExecutableNotFound)
    def extract_executable(self) -> pathlib.Path:
        return pathlib.Path(self._core_analyzer.get().extractExecutable())

    @intercept_runtime_errors(EngineError)
    def extract_failure_info(self) -> Dict[str, Any]:
        return self._core_analyzer.get().extractFailureInfo()

    @intercept_runtime_errors(EngineError)
    def extract_ps_info(self) -> Dict[str, Any]:
        return self._core_analyzer.get().extractPSInfo()

    cdef _is_ignored_lib(self, object path):
        return any(prefix in str(path) for prefix in self.ignored_libs)

    @intercept_runtime_errors(EngineError)
    def missing_modules(self) -> List[str]:
        cdef set result = set()
        cdef set missing_mod_names = set()
        for mod in self._core_analyzer.get().missingModules():
            path = pathlib.Path(mod)
            if not self._is_ignored_lib(path):
                result.add(path)
                missing_mod_names.add(path.name)
        for memmap in self._core_analyzer.get().MemoryMaps():
            path = pathlib.Path(memmap.path)
            if path.exists() or self._is_ignored_lib(path):
                continue
            if path.name not in missing_mod_names:
                result.add(path)
        return result

    @intercept_runtime_errors(EngineError)
    def extract_module_load_points(self) -> Dict[str, int]:
        return {
            pathlib.Path(mod.filename).name: mod.start
            for mod in self._core_analyzer.get().ModuleInformation()
        }

    @intercept_runtime_errors(EngineError)
    def extract_build_ids(self) -> Tuple[str, str, str]:
        cdef object memory_maps = self._core_analyzer.get().MemoryMaps()
        cdef object module_information = self._core_analyzer.get().ModuleInformation()
        memory_maps_by_file = {map['path']: map['buildid'] for map in memory_maps}
        for module in module_information:
            filename = module['filename']
            if self._is_ignored_lib(filename):
                continue
            mod_buildid = module['buildid']
            map_buildid = memory_maps_by_file.get(filename)
            yield (filename, mod_buildid, map_buildid)

cdef class ProcessManager:
    cdef shared_ptr[AbstractProcessManager] _manager

    cdef public object pid
    cdef public object python_version
    cdef public object virtual_maps
    cdef public object map_info

    def __init__(self, pid, python_version, memory_maps, map_info):
        self.pid = pid
        self.python_version = python_version
        self.virtual_maps = memory_maps
        self.map_info = map_info

    @classmethod
    def create_from_pid(cls, int pid, bint stop_process):
        virtual_maps = list(generate_maps_for_process(pid))
        map_info = parse_maps_file(pid, virtual_maps)

        cdef shared_ptr[NativeProcessAnalyzer] analyzer = make_shared[
            NativeProcessAnalyzer
        ](pid)
        cdef shared_ptr[AbstractProcessManager] native_manager = <shared_ptr[AbstractProcessManager]> (
            make_shared[NativeProcessManager](
                pid, stop_process, analyzer,
                _pymaps_to_maps(virtual_maps),
                _pymapinfo_to_mapinfo(map_info),
            )
        )

        python_version = native_manager.get().findPythonVersion()
        if python_version == (-1, -1):
            python_version = get_python_version_for_process(pid, map_info)
        native_manager.get().setPythonVersion(python_version)

        cdef ProcessManager new_manager = cls(
            pid, python_version, virtual_maps, map_info
        )
        new_manager._manager = native_manager
        return new_manager

    @classmethod
    def create_from_core(
        cls,
        core_file: pathlib.Path,
        executable: pathlib.Path,
        lib_search_path: Optional[pathlib.Path],
    ):
        cdef shared_ptr[NativeCoreFileAnalyzer] analyzer = get_core_analyzer(
            core_file, executable, lib_search_path
        )
        cdef unique_ptr[CoreFileExtractor] core_extractor = make_unique[
            CoreFileExtractor
        ](analyzer)

        mapped_files = core_extractor.get().extractMappedFiles()
        memory_maps = core_extractor.get().MemoryMaps()
        load_point_by_module = {
            pathlib.Path(mod.filename).name: mod.start
            for mod in core_extractor.get().ModuleInformation()
        }

        virtual_maps = list(
            generate_maps_from_core_data(mapped_files, memory_maps)
        )
        pid = core_extractor.get().Pid()
        map_info = parse_maps_file_for_binary(executable, virtual_maps, load_point_by_module)

        the_core_file = str(core_file)
        the_executable = str(executable)
        maps = _pymaps_to_maps(virtual_maps)
        native_map_info =  _pymapinfo_to_mapinfo(map_info)
        cdef shared_ptr[AbstractProcessManager] native_manager = <shared_ptr[AbstractProcessManager]> (
            make_shared[CoreFileProcessManager](pid, analyzer, maps, native_map_info)
        )

        python_version = native_manager.get().findPythonVersion()
        if python_version == (-1, -1):
            python_version = get_python_version_for_core(core_file, executable, map_info)
        native_manager.get().setPythonVersion(python_version)
        cdef ProcessManager new_manager = cls(
            pid, python_version, virtual_maps, map_info
        )
        new_manager._manager = native_manager

        return new_manager

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._manager.reset()

    cdef shared_ptr[AbstractProcessManager] get_manager(self):
        assert self._manager.get() != NULL
        return self._manager

    def interpreter_status(self) -> int:
        return self._manager.get().isInterpreterActive()

    def is_interpreter_active(self) -> bool:
        return self._manager.get().isInterpreterActive() == InterpreterStatus.RUNNING


######################################
# COMMON STACK-RETRIEVING FUNCTIONS  #
######################################

cdef object _try_to_decode_string(const cppstring *the_string):
    return PyUnicode_Decode(the_string.c_str(), the_string.size(), NULL, "replace")

cdef object _safe_cppmap_to_py(unordered_map[cppstring, cppstring] themap):
    cdef unordered_map[cppstring, cppstring] . iterator it = themap.begin()
    cdef dict result = {}
    while it != themap.end():
        key = _try_to_decode_string(&(dereference(it).first))
        val = _try_to_decode_string(&(dereference(it).second))
        result[key] = val
        postincrement(it)

    return result

cdef object _construct_frame_stack_from_thread_object(
    ssize_t pid, bint resolve_locals, FrameObject *first_frame
):
    cdef CodeObject *current_code = NULL
    cdef FrameObject *current_frame = first_frame

    last_frame = None

    while current_frame != NULL:
        current_code = current_frame.Code().get()

        filename = current_code.Filename()
        location_info = LocationInfo(
                current_code.Location().lineno,
                current_code.Location().end_lineno,
                current_code.Location().column,
                current_code.Location().end_column,
        )
        py_code = PyCodeObject(filename, current_code.Scope(), location_info)

        if resolve_locals:
            current_frame.resolveLocalVariables()

        args = _safe_cppmap_to_py(current_frame.Arguments())
        locals = _safe_cppmap_to_py(current_frame.Locals())
        is_entry = current_frame.IsEntryFrame()
        py_frame = PyFrame(None, None, py_code, args, locals, is_entry)

        py_frame.next = last_frame
        if last_frame:
            last_frame.prev = py_frame

        last_frame = py_frame
        current_frame = (
            current_frame.PreviousFrame().get()
            if current_frame.PreviousFrame()
            else NULL
        )

    return last_frame

cdef object _construct_threads_from_interpreter_state(
    shared_ptr[AbstractProcessManager] manager,
    remote_addr_t head,
    int pid,
    object python_version,
    bint add_native_traces,
    bint resolve_locals,
):
    LOGGER.info("Fetching Python threads")
    threads = []

    cdef shared_ptr[Thread] thread = getThreadFromInterpreterState(manager, head)
    cdef Thread *current_thread = thread.get()
    while current_thread != NULL:
        LOGGER.info("Constructing new Python thread with tid %s", current_thread.Tid())
        if add_native_traces:
            current_thread.populateNativeStackTrace(manager)
        frame = _construct_frame_stack_from_thread_object(
            pid, resolve_locals, current_thread.FirstFrame().get()
        )
        native_frames = [
            NativeFrame(**native_frame)
            for native_frame in list(current_thread.NativeFrames())
        ]
        threads.append(
            PyThread(
                current_thread.Tid(),
                frame,
                native_frames[::-1],
                current_thread.isGilHolder(),
                current_thread.isGCCollecting(),
                python_version,
                name=get_thread_name(pid, current_thread.Tid()),
            )
        )
        current_thread = (
            current_thread.NextThread().get() if current_thread.NextThread() else NULL
        )

    return threads

cdef object _construct_os_thread(
    shared_ptr[AbstractProcessManager] manager, int pid, int tid
):
    cdef unique_ptr[NativeThread] thread = make_unique[NativeThread](pid, tid)
    thread.get().populateNativeStackTrace(manager)
    native_frames = [
        NativeFrame(**native_frame)
        for native_frame in list(thread.get().NativeFrames())
    ]
    LOGGER.info("Constructing new native thread with tid %s", tid)
    pythread = PyThread(
        tid,
        None,
        native_frames[::-1],
        False,
        False,
        None,
        name=get_thread_name(pid, tid),
    )

    return pythread

cdef object _construct_os_threads(
    shared_ptr[AbstractProcessManager] manager, int pid, object tids
):
    LOGGER.info("Fetching native threads")
    threads = []
    for tid in tids:
        threads.append(_construct_os_thread(manager, pid, tid))

    return threads

cdef remote_addr_t _get_interpreter_state_addr(
    AbstractProcessManager *manager, object method, int core=False
) except*:
    cdef remote_addr_t head = 0
    possible_methods = [
        StackMethod.ELF_DATA,
        StackMethod.SYMBOLS,
        StackMethod.BSS,
        StackMethod.ANONYMOUS_MAPS,
        StackMethod.HEAP,
    ]

    for possible_method in possible_methods:
        if method.value & possible_method.value == 0:
            continue

        try:
            if possible_method == StackMethod.ELF_DATA:
                how = "using ELF data"
                head = manager.findInterpreterStateFromElfData()
            elif possible_method == StackMethod.SYMBOLS:
                how = "using symbols"
                head = manager.findInterpreterStateFromSymbols()
            elif possible_method == StackMethod.BSS:
                how = "scanning the BSS"
                head = manager.scanBSS()
            elif possible_method == StackMethod.ANONYMOUS_MAPS:
                how = "scanning all anonymous maps"
                head = manager.scanAllAnonymousMaps()
            elif possible_method == StackMethod.HEAP:
                how = "scanning the heap"
                head = manager.scanHeap()
        except Exception as exc:
            LOGGER.warning(
                "Unexpected error finding PyInterpreterState by %s: %s", how, exc
            )

        if head:
            LOGGER.info("PyInterpreterState found by %s at address 0x%0.2X", how, head)
            return head
        else:
            LOGGER.info("Address of PyInterpreterState not found by %s", how)

    LOGGER.info("Address of PyInterpreterState could not be found")
    return 0


def _check_interpreter_shutdown(manager):
    status = manager.interpreter_status()
    if status == InterpreterStatus.UNKNOWN:
        return
    if status == InterpreterStatus.FINALIZED:
        msg = (
            "The interpreter is shutting itself down so it is possible that no Python"
            " stack trace is available for inspection. You can still use --native-all "
            " to force displaying all the threads."
        )
        LOGGER.warning(msg)
    else:
        LOGGER.info("An active interpreter has been detected")


#####################
# PROCESS FUNCTIONS #
#####################


def _get_process_threads(
    pymanager: ProcessManager,
    pid: int,
    native_mode: NativeReportingMode,
    resolve_locals: bool,
    method: StackMethod,
):
    LOGGER.debug("Available memory maps for process:")
    for mem_map in pymanager.virtual_maps:
        LOGGER.debug(mem_map)

    cdef shared_ptr[AbstractProcessManager] manager = pymanager.get_manager()

    if native_mode != NativeReportingMode.ALL:
        _check_interpreter_shutdown(pymanager)

    cdef remote_addr_t head = _get_interpreter_state_addr(manager.get(), method)

    if not head and native_mode != NativeReportingMode.ALL:
        raise NotEnoughInformation(
            "Could not gather enough information to extract the Python frame information"
        )

    all_tids = list(manager.get().Tids())
    if head:
        add_native_traces = native_mode != NativeReportingMode.OFF
        for thread in _construct_threads_from_interpreter_state(
            manager,
            head,
            pid,
            pymanager.python_version,
            add_native_traces,
            resolve_locals,
        ):
            if thread.tid in all_tids:
                all_tids.remove(thread.tid)
            yield thread

    if native_mode == NativeReportingMode.ALL:
        yield from _construct_os_threads(manager, pid, all_tids)


def get_process_threads(
    pid: int,
    stop_process: bool = True,
    native_mode: NativeReportingMode = NativeReportingMode.OFF,
    locals: bool = False,
    method: StackMethod = StackMethod.AUTO,
) -> Iterable[PyThread]:
    """Return an iterable of Thread objects that are registered with the remote interpreter

        Args:
            pid (int): The pid of the remote process
            stop_process (bool): If *True*, stop the process for analysis and use
                blocking APis to obtain remote information.
            native_mode (NativeReportingMode): If set to PYTHON, include the
                native (C/C++) stack in the returned Thread objects for all threads
                registered with the interpreter. If set to ALL, native stacks
                from threads not registered with the interpreter will be provided
                as well. By default this is set to OFF and native stacks are not
                returned.
            locals (bool): If **True**, retrieve the local variables and arguments for
                every retrieved frame (may slow down the processing).
            method (StackMethod): The method to locate the relevant Python structs
                that are needed to unwind the Python stack.

        Returns:
            Iterable of Thread objects.
    """
    if not isinstance(method, StackMethod):
        raise ValueError("Invalid method for stack analysis")

    LOGGER.info(
        "Analyzing process with pid %s using stack method %s with native mode %s",
        pid,
        method,
        native_mode,
    )
    virtual_maps = list(generate_maps_for_process(pid))
    map_info = parse_maps_file(pid, virtual_maps)

    try:
        with ProcessManager.create_from_pid(pid, stop_process) as manager:
            yield from _get_process_threads(manager, pid, native_mode, locals, method)
    except RuntimeError as e:
        raise EngineError(*e.args, pid=pid) from e


######################
# COREFILE FUNCTIONS #
######################


def get_process_threads_for_core(
    core_file: pathlib.Path,
    executable: pathlib.Path,
    library_search_path: str = None,
    native_mode: NativeReportingMode = NativeReportingMode.PYTHON,
    locals: bool = False,
    method: StackMethod = StackMethod.AUTO,
) -> Iterable[PyThread]:
    """Return an iterable of Thread objects that are registered with the given core file

        Args:
            core_file (pathlib.Path): The location of the core file to analyze.
            executable (pathlib.Path): The location of the executable that the core file
                was created from.
            library_search_path (str): A ":"-separated list of directories to use when
                trying to locate missing shared libraries in the core file.
            native_mode (NativeReportingMode): If set to PYTHON, include the
                native (C/C++) stack in the returned Thread objects for all threads
                registered with the interpreter. If set to ALL, native stacks
                from threads not registered with the interpreter will be provided
                as well. By default this is set to OFF and native stacks are not
                returned.
            locals (bool): If **True**, retrieve the local variables and arguments for
                every retrieved frame (may slow down the processing).
            method (StackMethod): The method to locate the relevant Python structs
                that are needed to unwind the Python stack.

        Returns:
            Iterable of Thread objects.
    """
    if not isinstance(method, StackMethod):
        raise ValueError("Invalid method for stack analysis")

    LOGGER.info(
        "Analyzing core file %s with executable %s using stack method %s with native mode %s",
        core_file,
        executable,
        method,
        native_mode,
    )
    try:
        yield from _get_process_threads_for_core(
            core_file, executable, library_search_path, native_mode, locals, method
        )
    except RuntimeError as e:
        raise EngineError(*e.args, corefile=core_file) from e


def _get_process_threads_for_core(
    corefile: pathlib.Path,
    executable: pathlib.Path,
    library_search_path: str = None,
    native_mode: NativeReportingMode = NativeReportingMode.PYTHON,
    locals: bool = False,
    method: StackMethod = StackMethod.AUTO,
) -> Iterable[PyThread]:
    cdef ProcessManager pymanager = ProcessManager.create_from_core(
        corefile, executable, library_search_path
    )

    LOGGER.debug("Available memory maps for core:")
    for mem_map in pymanager.virtual_maps:
        LOGGER.debug(mem_map)

    cdef shared_ptr[AbstractProcessManager] manager = pymanager.get_manager()

    if native_mode != NativeReportingMode.ALL:
        _check_interpreter_shutdown(pymanager)

    cdef remote_addr_t head = _get_interpreter_state_addr(
        manager.get(), method, core=True
    )

    if not head and native_mode != NativeReportingMode.ALL:
        raise NotEnoughInformation(
            "Could not gather enough information to extract the Python frame information"
        )

    all_tids = list(manager.get().Tids())

    if head:
        native = native_mode in {NativeReportingMode.PYTHON, NativeReportingMode.ALL}
        for thread in _construct_threads_from_interpreter_state(
            manager, head, pymanager.pid, pymanager.python_version, native, locals
        ):
            if thread.tid in all_tids:
                all_tids.remove(thread.tid)
            yield thread

    if native_mode == NativeReportingMode.ALL:
        yield from _construct_os_threads(manager, pymanager.pid, all_tids)
