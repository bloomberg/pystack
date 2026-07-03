from typing import Any
from typing import Dict
from typing import List
from typing import Optional
from typing import Tuple

from pystack._pystack import NativeReportingMode
from pystack._pystack import _normalize_threads_for_testing

EVAL = "_PyEval_EvalFrameDefault"
PY_VERSION = (3, 13)


def _make_thread(
    tid: int,
    *,
    stack_anchor: int = 0,
    interpreter_id: int = 0,
    native_symbols: Optional[List[str]] = None,
    frames: Optional[List[Tuple[str, bool]]] = None,
) -> Dict[str, Any]:
    return dict(
        tid=tid,
        stack_anchor=stack_anchor,
        interpreter_id=interpreter_id,
        native_symbols=(native_symbols or []),
        frames=(frames or []),
        python_version=PY_VERSION,
    )


def test_unique_tids_pass_through_in_order():
    threads = [
        _make_thread(
            3,
            native_symbols=["alpha", EVAL, "beta"],
            frames=[("main", True)],
        ),
        _make_thread(
            1,
            native_symbols=["gamma", EVAL, "delta"],
            frames=[("run", True)],
        ),
        _make_thread(
            2,
            native_symbols=["epsilon"],
            frames=[("work", True)],
        ),
    ]

    result = _normalize_threads_for_testing(threads, NativeReportingMode.PYTHON)

    assert len(result) == 3
    assert [t.tid for t in result] == [3, 1, 2]
    assert [f.symbol for f in result[0].native_frames] == ["alpha", EVAL, "beta"]
    assert [f.symbol for f in result[1].native_frames] == ["gamma", EVAL, "delta"]
    assert [f.symbol for f in result[2].native_frames] == ["epsilon"]
    assert [f.code.scope for f in result[0].frames] == ["main"]
    assert [f.code.scope for f in result[1].frames] == ["run"]
    assert [f.code.scope for f in result[2].frames] == ["work"]


def test_first_seen_tid_order_preserved():
    threads = [
        _make_thread(10, interpreter_id=0, stack_anchor=1000, frames=[("a", True)]),
        _make_thread(20, interpreter_id=0, stack_anchor=2000, frames=[("b", True)]),
        _make_thread(20, interpreter_id=1, stack_anchor=1500, frames=[("d", True)]),
        _make_thread(10, interpreter_id=1, stack_anchor=500, frames=[("c", True)]),
    ]

    result = _normalize_threads_for_testing(threads, NativeReportingMode.OFF)

    assert [t.tid for t in result] == [10, 10, 20, 20]
    assert [t.interpreter_id for t in result] == [0, 1, 0, 1]
    assert [f.code.scope for f in result[0].frames] == ["a"]
    assert [f.code.scope for f in result[1].frames] == ["c"]
    assert [f.code.scope for f in result[2].frames] == ["b"]
    assert [f.code.scope for f in result[3].frames] == ["d"]


def test_stack_anchor_sort_within_group():
    threads = [
        _make_thread(1, interpreter_id=2, stack_anchor=0, frames=[("inner", True)]),
        _make_thread(1, interpreter_id=0, stack_anchor=9000, frames=[("outer", True)]),
        _make_thread(1, interpreter_id=1, stack_anchor=5000, frames=[("middle", True)]),
    ]

    result = _normalize_threads_for_testing(threads, NativeReportingMode.OFF)

    assert len(result) == 3
    assert [t.interpreter_id for t in result] == [0, 1, 2]
    assert [f.code.scope for f in result[0].frames] == ["outer"]
    assert [f.code.scope for f in result[1].frames] == ["middle"]
    assert [f.code.scope for f in result[2].frames] == ["inner"]


def test_native_slice_correctness():
    native_symbols = [
        "outer_c_func",
        EVAL,
        "middle_c_func_a",
        "middle_c_func_b",
        EVAL,
        "inner_c_func",
        EVAL,
    ]
    threads = [
        _make_thread(
            1,
            interpreter_id=0,
            stack_anchor=9000,
            native_symbols=native_symbols,
            frames=[("helper", False), ("main", True)],
        ),
        _make_thread(
            1,
            interpreter_id=1,
            stack_anchor=5000,
            native_symbols=native_symbols,
            frames=[("run", True)],
        ),
        _make_thread(
            1,
            interpreter_id=2,
            stack_anchor=1000,
            native_symbols=native_symbols,
            frames=[("work", True)],
        ),
    ]

    result = _normalize_threads_for_testing(threads, NativeReportingMode.PYTHON)

    assert len(result) == 3
    assert result[0].interpreter_id == 0
    assert result[1].interpreter_id == 1
    assert result[2].interpreter_id == 2

    syms0 = [f.symbol for f in result[0].native_frames]
    syms1 = [f.symbol for f in result[1].native_frames]
    syms2 = [f.symbol for f in result[2].native_frames]

    assert syms0 == ["outer_c_func", EVAL, "middle_c_func_a", "middle_c_func_b"]
    assert syms1 == [EVAL, "inner_c_func"]
    assert syms2 == [EVAL]

    assert syms0 + syms1 + syms2 == native_symbols

    assert [f.code.scope for f in result[0].frames] == ["main", "helper"]
    assert [f.code.scope for f in result[1].frames] == ["run"]
    assert [f.code.scope for f in result[2].frames] == ["work"]


def test_middle_interpreter_no_frames_gets_native_cleared():
    native_symbols = [
        "setup",
        EVAL,
        "bridge",
        EVAL,
    ]
    threads = [
        _make_thread(
            1,
            interpreter_id=0,
            stack_anchor=9000,
            native_symbols=native_symbols,
            frames=[("outer", True)],
        ),
        _make_thread(
            1,
            interpreter_id=1,
            stack_anchor=5000,
            native_symbols=native_symbols,
            frames=[],
        ),
        _make_thread(
            1,
            interpreter_id=2,
            stack_anchor=1000,
            native_symbols=native_symbols,
            frames=[("inner", True)],
        ),
    ]

    result = _normalize_threads_for_testing(threads, NativeReportingMode.PYTHON)

    assert len(result) == 3
    assert result[0].interpreter_id == 0
    assert result[1].interpreter_id == 1
    assert result[2].interpreter_id == 2

    assert [f.symbol for f in result[0].native_frames] == ["setup", EVAL, "bridge"]
    assert [f.symbol for f in result[1].native_frames] == []
    assert [f.symbol for f in result[2].native_frames] == [EVAL]

    assert [f.code.scope for f in result[0].frames] == ["outer"]
    assert [f.code.scope for f in result[1].frames] == []
    assert [f.code.scope for f in result[2].frames] == ["inner"]
