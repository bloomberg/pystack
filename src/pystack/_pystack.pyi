import enum
import pathlib
from typing import Any
from typing import Dict
from typing import Iterable
from typing import List
from typing import Optional
from typing import Tuple
from typing import Union

from .types import PyThread

class CoreFileAnalyzer:
    @classmethod
    def __init__(cls, *args: Any, **kwargs: Any) -> None: ...
    def extract_module_load_points(self) -> Dict[str, int]: ...
    def extract_build_ids(self) -> Iterable[Tuple[str, str, str]]: ...
    def extract_executable(self) -> pathlib.Path: ...
    def extract_failure_info(self) -> Dict[str, Any]: ...
    def extract_maps(self) -> Iterable[Dict[str, Any]]: ...
    def extract_pid(self) -> int: ...
    def extract_ps_info(self) -> Dict[str, Any]: ...
    def missing_modules(self) -> List[str]: ...

class NativeReportingMode(enum.Enum):
    ALL: int
    OFF: int
    PYTHON: int

class StackMethod(enum.Enum):
    ALL: int
    ANONYMOUS_MAPS: int
    AUTO: int
    BSS: int
    ELF_DATA: int
    HEAP: int
    SYMBOLS: int

class ProcessManager: ...

def get_process_threads(
    pid: int,
    stop_process: bool = True,
    native_mode: NativeReportingMode = NativeReportingMode.OFF,
    locals: bool = False,
    method: StackMethod = StackMethod.AUTO,
) -> Iterable[PyThread]: ...
def get_process_threads_for_core(
    core_file: pathlib.Path,
    executable: pathlib.Path,
    library_search_path: Optional[str] = None,
    native_mode: NativeReportingMode = NativeReportingMode.PYTHON,
    locals: bool = False,
    method: StackMethod = StackMethod.AUTO,
) -> Iterable[PyThread]: ...
def get_bss_info(binary: Union[str, pathlib.Path]) -> Dict[str, Any]: ...
def copy_memory_from_address(
    pid: int, address: int, size: int, blocking: bool = False
) -> bytes: ...
