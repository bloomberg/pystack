import enum
import pathlib
from typing import Any
from typing import Callable
from typing import Dict
from typing import Iterable
from typing import List
from typing import Optional
from typing import Tuple
from typing import TypeVar
from typing import Union

from .types import PyThread

class CoreFileAnalyzer:
    def __init__(
        self,
        core_file: Union[str, pathlib.Path],
        executable: Optional[Union[str, pathlib.Path]] = None,
        lib_search_path: Optional[str] = None,
    ) -> None: ...
    def extract_module_load_points(self) -> Dict[str, int]: ...
    def extract_build_ids(self) -> Iterable[Tuple[str, str, str]]: ...
    def extract_executable(self) -> pathlib.Path: ...
    def extract_failure_info(self) -> Dict[str, Any]: ...
    def extract_maps(self) -> List[Dict[str, Any]]: ...
    def extract_pid(self) -> int: ...
    def extract_ps_info(self) -> Dict[str, Any]: ...
    def missing_modules(self) -> List[str]: ...

class NativeReportingMode(enum.Enum):
    OFF = 0
    PYTHON = 1
    ALL = 1000
    LAST = 2000

class StackMethod(enum.Enum):
    ELF_DATA = 1
    SYMBOLS = 2
    BSS = 4
    ANONYMOUS_MAPS = 8
    HEAP = 16
    DEBUG_OFFSETS = 32
    AUTO = 55  # DEBUG_OFFSETS | ELF_DATA | SYMBOLS | BSS
    ALL = 63  # AUTO | ANONYMOUS_MAPS | HEAP

class ProcessManager:
    pid: int
    python_version: Tuple[int, int]

    @classmethod
    def create_from_pid(
        cls, pid: int, stop_process: bool = True
    ) -> "ProcessManager": ...
    @classmethod
    def create_from_core(
        cls,
        core_file: Union[str, pathlib.Path],
        executable: Union[str, pathlib.Path],
        lib_search_path: Optional[str] = None,
    ) -> "ProcessManager": ...
    def interpreter_status(self) -> int: ...
    def is_interpreter_active(self) -> bool: ...
    def __enter__(self) -> "ProcessManager": ...
    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None: ...

def get_process_threads(
    pid: int,
    stop_process: bool = True,
    native_mode: NativeReportingMode = NativeReportingMode.OFF,
    locals: bool = False,
    method: StackMethod = StackMethod.AUTO,
) -> List[PyThread]: ...
def get_process_threads_for_core(
    core_file: Union[str, pathlib.Path],
    executable: Union[str, pathlib.Path],
    library_search_path: Optional[str] = None,
    native_mode: NativeReportingMode = NativeReportingMode.PYTHON,
    locals: bool = False,
    method: StackMethod = StackMethod.AUTO,
) -> List[PyThread]: ...
def get_bss_info(binary: Union[str, pathlib.Path]) -> Optional[Dict[str, Any]]: ...
def copy_memory_from_address(pid: int, address: int, size: int) -> bytes: ...
def _check_interpreter_shutdown(manager: ProcessManager) -> None: ...

F = TypeVar("F", bound=Callable[..., Any])

def intercept_runtime_errors() -> Callable[[F], F]: ...
