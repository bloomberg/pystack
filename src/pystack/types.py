import enum
from dataclasses import dataclass
from typing import Dict
from typing import Iterable
from typing import List
from typing import NamedTuple
from typing import Optional
from typing import Tuple

SYMBOL_IGNORELIST = {
    "PyObject_Call",
    "call_function",
    "classmethoddescr_call",
    "cmpwrapper_call",
    "fast_function",
    "function_call",
    "instance_call",
    "instancemethod_call",
    "instancemethod_call",
    "methoddescr_call",
    "proxy_call",
    "slot_tp_call",
    "type_call",
    "weakref_call",
    "wrap_call",
    "wrapper_call",
    "wrapperdescr_call",
    "do_call_core",
}


@dataclass
class NativeFrame:
    address: int
    symbol: str
    path: str
    linenumber: int
    colnumber: int
    library: str

    class FrameType(enum.Enum):
        IGNORE = 0
        EVAL = 1
        OTHER = 3


def _is_eval_frame(symbol: str, python_version: Tuple[int, int]) -> bool:
    if python_version < (3, 6):
        return "PyEval_EvalFrameEx" in symbol
    return "_PyEval_EvalFrameDefault" in symbol


def frame_type(
    frame: NativeFrame, python_version: Optional[Tuple[int, int]]
) -> NativeFrame.FrameType:
    symbol = frame.symbol
    if python_version and _is_eval_frame(symbol, python_version):
        return frame.FrameType.EVAL
    if symbol.startswith("PyEval") or symbol.startswith("_PyEval"):
        return frame.FrameType.IGNORE
    if symbol.startswith("_Py"):
        return frame.FrameType.IGNORE
    if python_version and python_version >= (3, 8) and "vectorcall" in symbol.lower():
        return frame.FrameType.IGNORE
    if any(symbol.startswith(ignored_symbol) for ignored_symbol in SYMBOL_IGNORELIST):
        return frame.FrameType.IGNORE

    return frame.FrameType.OTHER


class LocationInfo(NamedTuple):
    lineno: int
    end_lineno: int
    column: int
    end_column: int


@dataclass
class PyCodeObject:
    filename: str
    scope: str
    location: LocationInfo


@dataclass
class LocalVariable:
    name: int
    value: str
    is_argument: bool


@dataclass
class PyFrame:
    prev: Optional["PyFrame"]
    next: Optional["PyFrame"]
    code: PyCodeObject
    arguments: Dict[str, str]
    locals: Dict[str, str]
    is_entry: bool


@dataclass
class PyThread:
    tid: int
    frame: Optional[PyFrame]
    native_frames: List[NativeFrame]
    holds_the_gil: int
    is_gc_collecting: int
    python_version: Optional[Tuple[int, int]]
    name: Optional[str] = None

    @property
    def frames(self) -> Iterable[PyFrame]:
        current_frame = self.frame
        while current_frame is not None:
            yield current_frame
            current_frame = current_frame.next

    @property
    def status(self) -> str:
        status = []
        gil_status = self.gil_status
        gc_status = self.gc_status
        if self.tid == 0:
            status.append("Thread terminated")
        if gil_status:
            status.append(gil_status)
        if gc_status:
            status.append(gc_status)
        return "[" + ",".join(status) + "]"

    @property
    def gc_status(self) -> str:
        if self.native_frames:
            gc_symbols = {"gc_collect", "collect", "collect.constrprop"}
            if any(
                gc_symbol in frame.symbol
                for gc_symbol in gc_symbols
                for frame in self.native_frames
            ):
                return "Garbage collecting"
            return ""

        if self.is_gc_collecting != -1:
            if self.is_gc_collecting == 1 and self.holds_the_gil:
                return "Garbage collecting"
            return ""

        return ""

    @property
    def gil_status(self) -> str:
        if self.holds_the_gil > 0:
            return "Has the GIL"
        if any(frame.symbol == "take_gil" for frame in self.native_frames):
            return "Waiting for the GIL"
        if any(frame.symbol == "drop_gil" for frame in self.native_frames):
            return "Dropping the GIL"
        return ""
