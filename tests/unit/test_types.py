import pytest

from pystack.types import SYMBOL_IGNORELIST
from pystack.types import NativeFrame
from pystack.types import PyThread
from pystack.types import frame_type


@pytest.mark.parametrize(
    "symbol", ["_PyEval_EvalFrameDefault", "_PyEval_EvalFrameDefault.cold.32"]
)
@pytest.mark.parametrize(
    "version, expected",
    [
        ((2, 7), NativeFrame.FrameType.IGNORE),
        ((3, 5), NativeFrame.FrameType.IGNORE),
        ((3, 6), NativeFrame.FrameType.EVAL),
        ((3, 7), NativeFrame.FrameType.EVAL),
        ((3, 8), NativeFrame.FrameType.EVAL),
        ((3, 9), NativeFrame.FrameType.EVAL),
    ],
    ids=["2.7", "3.5", "3.6", "3.7", "3.8", "3.9"],
)
def test_frame_type_eval_frame_with_pep_523(symbol, version, expected):
    # GIVEN

    frame = NativeFrame(0x3, symbol, "Python/ceval.c", 123, 0, "library.so")

    # WHEN

    type_ = frame_type(frame, version)

    # THEN

    assert type_ == expected


@pytest.mark.parametrize("symbol", ["PyEval_EvalFrameEx", "PyEval_EvalFrameEx.cold.32"])
@pytest.mark.parametrize(
    "version, expected",
    [
        ((2, 7), NativeFrame.FrameType.EVAL),
        ((3, 5), NativeFrame.FrameType.EVAL),
        ((3, 6), NativeFrame.FrameType.IGNORE),
        ((3, 7), NativeFrame.FrameType.IGNORE),
        ((3, 8), NativeFrame.FrameType.IGNORE),
        ((3, 9), NativeFrame.FrameType.IGNORE),
    ],
    ids=["2.7", "3.5", "3.6", "3.7", "3.8", "3.9"],
)
def test_frame_type_eval_frame_without_pep_523(symbol, version, expected):
    # GIVEN

    frame = NativeFrame(
        0x3, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"
    )

    # WHEN

    type_ = frame_type(frame, version)

    # THEN

    assert type_ == expected


@pytest.mark.parametrize(
    "version",
    [(2, 7), (3, 5), (3, 6), (3, 7), (3, 8), (3, 9)],
    ids=["2.7", "3.5", "3.6", "3.7", "3.8", "3.9"],
)
def test_frame_type_eval_machinery_is_ignored(version):
    # GIVEN

    frame = NativeFrame(
        0x3, "_PyEval_SomeStuff", "Python/ceval.c", 123, 0, "library.so"
    )

    # WHEN

    type_ = frame_type(frame, version)

    # THEN

    assert type_ == NativeFrame.FrameType.IGNORE


@pytest.mark.parametrize(
    "version",
    [(2, 7), (3, 5), (3, 6), (3, 7), (3, 8), (3, 9)],
    ids=["2.7", "3.5", "3.6", "3.7", "3.8", "3.9"],
)
def test_frame_type_private_python_apis_are_ignored(version):
    # GIVEN

    frame = NativeFrame(
        0x3, "_PySome_Private_Api", "Python/private.c", 123, 0, "library.so"
    )

    # WHEN

    type_ = frame_type(frame, version)

    # THEN

    assert type_ == NativeFrame.FrameType.IGNORE


@pytest.mark.parametrize(
    "version, expected",
    [
        ((2, 7), NativeFrame.FrameType.OTHER),
        ((3, 5), NativeFrame.FrameType.OTHER),
        ((3, 6), NativeFrame.FrameType.OTHER),
        ((3, 7), NativeFrame.FrameType.OTHER),
        ((3, 8), NativeFrame.FrameType.IGNORE),
        ((3, 9), NativeFrame.FrameType.IGNORE),
    ],
    ids=["2.7", "3.5", "3.6", "3.7", "3.8", "3.9"],
)
def test_frame_type_vectorcall_machinery(version, expected):
    # GIVEN

    frame = NativeFrame(
        0x3, "blablabla_vectorcall_blublublu", "Python/private.c", 123, 0, "library.so"
    )

    # WHEN

    type_ = frame_type(frame, version)

    # THEN

    assert type_ == expected


@pytest.mark.parametrize("symbol", sorted(SYMBOL_IGNORELIST))
@pytest.mark.parametrize(
    "version",
    [(2, 7), (3, 5), (3, 6), (3, 7), (3, 8), (3, 9)],
    ids=["2.7", "3.5", "3.6", "3.7", "3.8", "3.9"],
)
def test_frame_type_explicitly_ignored_symbols_are_ignored(symbol, version):
    # GIVEN

    frame = NativeFrame(0x3, symbol, "Python/private.c", 123, 0, "library.so")

    # WHEN

    type_ = frame_type(frame, version)

    # THEN

    assert type_ == NativeFrame.FrameType.IGNORE


@pytest.mark.parametrize(
    "gil_state, expected_string",
    [
        (0, ""),
        (-1, ""),
        (1, "Has the GIL"),
    ],
)
def test_gil_states(gil_state, expected_string):
    # GIVEN

    thread = PyThread(1, None, [], gil_state, 0, (3, 8))

    # WHEN

    state = thread.gil_status

    # THEN

    assert state == expected_string


@pytest.mark.parametrize(
    "gc_state, expected_string",
    [
        (0, ""),
        (-1, ""),
        (1, "Garbage collecting"),
    ],
)
def test_gc_states_with_gil(gc_state, expected_string):
    # GIVEN

    thread = PyThread(1, None, [], 1, gc_state, (3, 8))

    # WHEN

    state = thread.gc_status

    # THEN

    assert state == expected_string


@pytest.mark.parametrize(
    "gc_state",
    [[0, -1, 1]],
)
def test_gc_states_with_no_gil(gc_state):
    # GIVEN

    thread = PyThread(1, None, [], 0, gc_state, (3, 8))

    # WHEN

    state = thread.gc_status

    # THEN

    assert state == ""


def test_dead_thread():
    # GIVEN

    thread = PyThread(0, None, [], 0, 0, (3, 8))

    # WHEN

    state = thread.status

    # THEN

    assert state == "[Thread terminated]"
