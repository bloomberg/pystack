from unittest.mock import mock_open
from unittest.mock import patch

import pytest

from pystack.traceback_formatter import format_thread
from pystack.traceback_formatter import print_thread
from pystack.types import SYMBOL_IGNORELIST
from pystack.types import LocationInfo
from pystack.types import NativeFrame
from pystack.types import PyCodeObject
from pystack.types import PyFrame
from pystack.types import PyThread


@pytest.fixture(autouse=True)
def no_color_env_var(monkeypatch):
    monkeypatch.setenv("NO_COLOR", "1")


def test_traceback_formatter_no_native():
    # GIVEN

    codes = [
        PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        PyCodeObject(
            filename="file2.py",
            scope="function2",
            location=LocationInfo(2, 2, 0, 0),
        ),
        PyCodeObject(
            filename="file3.py",
            scope="function3",
            location=LocationInfo(3, 3, 0, 0),
        ),
    ]

    current_frame = None
    for code in reversed(codes):
        current_frame = PyFrame(
            prev=None,
            next=current_frame,
            code=code,
            arguments={},
            locals={},
            is_entry=True,
        )

    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(0x1, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x2, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(0x3, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x4, "native_function3", "native_file3.c", 3, 0, "library.so"),
        NativeFrame(0x5, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x6, "native_function4", "native_file4.c", 4, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=current_frame,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=False, native_last=False))

    # THEN

    assert lines == [
        "Traceback for thread 1 [] (most recent call last):",
        '    (Python) File "file1.py", line 1, in function1',
        '    (Python) File "file2.py", line 2, in function2',
        '    (Python) File "file3.py", line 3, in function3',
        "",
    ]


def test_traceback_formatter_no_frames_no_native():
    # GIVEN

    thread = PyThread(
        tid=1,
        frame=None,
        native_frames=[],
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=False, native_last=False))

    # THEN

    assert lines == ["The frame stack for thread 1 is empty"]


def test_traceback_formatter_no_frames_native():
    # GIVEN

    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(0x2, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(0x4, "native_function3", "native_file3.c", 3, 0, "library.so"),
        NativeFrame(0x6, "native_function4", "native_file4.c", 4, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=None,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=True, native_last=False))

    # THEN
    assert lines == [
        "Traceback for thread 1 [] (most recent call last):",
        '    (C) File "native_file1.c", line 1, in native_function1 (library.so)',
        '    (C) File "native_file2.c", line 2, in native_function2 (library.so)',
        '    (C) File "native_file3.c", line 3, in native_function3 (library.so)',
        '    (C) File "native_file4.c", line 4, in native_function4 (library.so)',
        "",
    ]


def test_traceback_formatter_no_frames_native_with_eval_frames():
    # GIVEN

    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(0x1, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x2, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(0x3, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x4, "native_function3", "native_file3.c", 3, 0, "library.so"),
        NativeFrame(0x5, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x6, "native_function4", "native_file4.c", 4, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=None,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(2, 7),
    )

    # WHEN

    lines = list(format_thread(thread, native=True, native_last=False))

    # THEN
    assert lines == [
        "Traceback for thread 1 [] (most recent call last):",
        "* - Unable to merge native stack due to insufficient native information - *",
        "",
    ]


def test_traceback_formatter_no_mergeable_native_frames():
    # GIVEN

    codes = [
        PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        PyCodeObject(
            filename="file2.py",
            scope="function2",
            location=LocationInfo(2, 2, 0, 0),
        ),
        PyCodeObject(
            filename="file3.py",
            scope="function3",
            location=LocationInfo(3, 3, 0, 0),
        ),
    ]

    current_frame = None
    for code in reversed(codes):
        current_frame = PyFrame(
            prev=None,
            next=current_frame,
            code=code,
            arguments={},
            locals={},
            is_entry=True,
        )

    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(0x1, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x2, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(0x4, "native_function3", "native_file3.c", 3, 0, "library.so"),
        NativeFrame(0x5, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x6, "native_function4", "native_file4.c", 4, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=current_frame,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=True, native_last=False))

    # THEN
    assert lines == [
        "Traceback for thread 1 [] (most recent call last):",
        "* - Unable to merge native stack due to insufficient native information - *",
        '    (Python) File "file1.py", line 1, in function1',
        '    (Python) File "file2.py", line 2, in function2',
        '    (Python) File "file3.py", line 3, in function3',
        "",
    ]


def test_traceback_formatter_with_source():
    # GIVEN

    codes = [
        PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        PyCodeObject(
            filename="file2.py",
            scope="function2",
            location=LocationInfo(2, 2, 0, 0),
        ),
        PyCodeObject(
            filename="file3.py",
            scope="function3",
            location=LocationInfo(3, 3, 0, 0),
        ),
    ]

    current_frame = None
    for code in reversed(codes):
        current_frame = PyFrame(
            prev=None,
            next=current_frame,
            code=code,
            arguments={},
            locals={},
            is_entry=True,
        )

    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(0x1, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x2, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(0x3, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x4, "native_function3", "native_file3.c", 3, 0, "library.so"),
        NativeFrame(0x5, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x6, "native_function4", "native_file4.c", 4, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=current_frame,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN
    source_data = "\n".join(f'x = "This is the line {line}"' for line in range(1, 5))
    with patch("builtins.open", mock_open(read_data=source_data)), patch(
        "os.path.exists", return_value=True
    ):
        lines = list(format_thread(thread, native=False, native_last=False))

    # THEN

    assert lines == [
        "Traceback for thread 1 [] (most recent call last):",
        '    (Python) File "file1.py", line 1, in function1',
        '        x = "This is the line 1"',
        '    (Python) File "file2.py", line 2, in function2',
        '        x = "This is the line 2"',
        '    (Python) File "file3.py", line 3, in function3',
        '        x = "This is the line 3"',
        "",
    ]


def test_traceback_formatter_native_matching_simple_eval_frames():
    # GIVEN

    codes = [
        PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        PyCodeObject(
            filename="file2.py",
            scope="function2",
            location=LocationInfo(2, 2, 0, 0),
        ),
        PyCodeObject(
            filename="file3.py",
            scope="function3",
            location=LocationInfo(3, 3, 0, 0),
        ),
    ]

    current_frame = None
    for code in reversed(codes):
        current_frame = PyFrame(
            prev=None,
            next=current_frame,
            code=code,
            arguments={},
            locals={},
            is_entry=True,
        )

    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(
            0x1, "_PyEval_EvalFrameDefault", "Python/ceval.c", 123, 0, "library.so"
        ),
        NativeFrame(0x2, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(
            0x3, "_PyEval_EvalFrameDefault", "Python/ceval.c", 123, 0, "library.so"
        ),
        NativeFrame(0x4, "native_function3", "native_file3.c", 3, 0, "library.so"),
        NativeFrame(
            0x5, "_PyEval_EvalFrameDefault", "Python/ceval.c", 123, 0, "library.so"
        ),
        NativeFrame(0x6, "native_function4", "native_file4.c", 4, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=current_frame,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=True, native_last=False))

    # THEN

    assert lines == [
        "Traceback for thread 1 [] (most recent call last):",
        '    (C) File "native_file1.c", line 1, in native_function1 (library.so)',
        '    (Python) File "file1.py", line 1, in function1',
        '    (C) File "native_file2.c", line 2, in native_function2 (library.so)',
        '    (Python) File "file2.py", line 2, in function2',
        '    (C) File "native_file3.c", line 3, in native_function3 (library.so)',
        '    (Python) File "file3.py", line 3, in function3',
        '    (C) File "native_file4.c", line 4, in native_function4 (library.so)',
        "",
    ]


def test_traceback_formatter_native_matching_composite_eval_frames():
    # GIVEN

    codes = [
        PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        PyCodeObject(
            filename="file2.py",
            scope="function2",
            location=LocationInfo(2, 2, 0, 0),
        ),
        PyCodeObject(
            filename="file3.py",
            scope="function3",
            location=LocationInfo(3, 3, 0, 0),
        ),
    ]

    current_frame = None
    for code in reversed(codes):
        current_frame = PyFrame(
            prev=None,
            next=current_frame,
            code=code,
            arguments={},
            locals={},
            is_entry=True,
        )

    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(0x1, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(
            0x2, "PyEval_EvalFrameDefault", "Python/ceval.c", 12, 0, "library.so"
        ),
        NativeFrame(
            0x3, "_PyEval_EvalFrameDefault", "Python/ceval.c", 130, 0, "library.so"
        ),
        NativeFrame(0x4, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(0x5, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(
            0x7, "PyEval_EvalFrameDefault", "Python/ceval.c", 12, 0, "library.so"
        ),
        NativeFrame(
            0x8, "_PyEval_EvalFrameDefault", "Python/ceval.c", 130, 0, "library.so"
        ),
        NativeFrame(0x9, "native_function3", "native_file3.c", 3, 0, "library.so"),
        NativeFrame(0x10, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(
            0x11, "PyEval_EvalFrameDefault", "Python/ceval.c", 12, 0, "library.so"
        ),
        NativeFrame(
            0x12, "_PyEval_EvalFrameDefault", "Python/ceval.c", 130, 0, "library.so"
        ),
        NativeFrame(0x13, "native_function4", "native_file4.c", 4, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=current_frame,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=True, native_last=False))

    # THEN

    assert lines == [
        "Traceback for thread 1 [] (most recent call last):",
        '    (C) File "native_file1.c", line 1, in native_function1 (library.so)',
        '    (Python) File "file1.py", line 1, in function1',
        '    (C) File "native_file2.c", line 2, in native_function2 (library.so)',
        '    (Python) File "file2.py", line 2, in function2',
        '    (C) File "native_file3.c", line 3, in native_function3 (library.so)',
        '    (Python) File "file3.py", line 3, in function3',
        '    (C) File "native_file4.c", line 4, in native_function4 (library.so)',
        "",
    ]


def test_traceback_formatter_native_matching_eval_frames_ignore_frames():
    # GIVEN

    codes = [
        PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        PyCodeObject(
            filename="file2.py",
            scope="function2",
            location=LocationInfo(2, 2, 0, 0),
        ),
        PyCodeObject(
            filename="file3.py",
            scope="function3",
            location=LocationInfo(3, 3, 0, 0),
        ),
        PyCodeObject(
            filename="file4.py",
            scope="function4",
            location=LocationInfo(4, 4, 0, 0),
        ),
    ]

    current_frame = None
    for code in reversed(codes):
        current_frame = PyFrame(
            prev=None,
            next=current_frame,
            code=code,
            arguments={},
            locals={},
            is_entry=True,
        )

    ignorelist_frames = [
        NativeFrame(0x10, symbol, "some/random/file.c", 13, 0, "library.so")
        for symbol in SYMBOL_IGNORELIST
    ]

    private_frames = [
        NativeFrame(0x11, symbol, "Python/private.c", 13, 0, "library.so")
        for symbol in ("_PyPrivateFunction", "_PyAnotherPrivateFunction")
    ]

    eval_ignore_frames = [
        NativeFrame(0x12, symbol, "Python/ceval.c", 13, 0, "library.so")
        for symbol in ("PyEval_SomethingSomething", "_PyEval_SomethingSomething")
    ]

    vectorcall_frames = [
        NativeFrame(0x13, symbol, "Python/call.c", 13, 0, "library.so")
        for symbol in (
            "vectorcall_rules",
            "function_vectorcall",
            "super_Vectorcall_call",
            "VECTORCALL_Ex",
        )
    ]

    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(
            0x1, "_PyEval_EvalFrameDefault", "Python/ceval.c", 123, 0, "library.so"
        ),
        *eval_ignore_frames,
        NativeFrame(0x2, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(
            0x3, "_PyEval_EvalFrameDefault", "Python/ceval.c", 123, 0, "library.so"
        ),
        *ignorelist_frames,
        NativeFrame(0x4, "native_function3", "native_file3.c", 3, 0, "library.so"),
        NativeFrame(
            0x5, "_PyEval_EvalFrameDefault", "Python/ceval.c", 123, 0, "library.so"
        ),
        *private_frames,
        NativeFrame(0x6, "native_function4", "native_file4.c", 4, 0, "library.so"),
        NativeFrame(
            0x7, "_PyEval_EvalFrameDefault", "Python/ceval.c", 123, 0, "library.so"
        ),
        *vectorcall_frames,
        NativeFrame(0x8, "native_function5", "native_file5.c", 5, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=current_frame,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=True, native_last=False))

    # THEN

    assert lines == [
        "Traceback for thread 1 [] (most recent call last):",
        '    (C) File "native_file1.c", line 1, in native_function1 (library.so)',
        '    (Python) File "file1.py", line 1, in function1',
        '    (C) File "native_file2.c", line 2, in native_function2 (library.so)',
        '    (Python) File "file2.py", line 2, in function2',
        '    (C) File "native_file3.c", line 3, in native_function3 (library.so)',
        '    (Python) File "file3.py", line 3, in function3',
        '    (C) File "native_file4.c", line 4, in native_function4 (library.so)',
        '    (Python) File "file4.py", line 4, in function4',
        '    (C) File "native_file5.c", line 5, in native_function5 (library.so)',
        "",
    ]


def test_traceback_formatter_gil_detection():
    # GIVEN

    frame = PyFrame(
        prev=None,
        next=None,
        code=PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        arguments={},
        locals={},
        is_entry=True,
    )
    thread = PyThread(
        tid=1,
        frame=frame,
        native_frames=[],
        holds_the_gil=True,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=False, native_last=False))

    # THEN

    assert lines == [
        "Traceback for thread 1 [Has the GIL] (most recent call last):",
        '    (Python) File "file1.py", line 1, in function1',
        "",
    ]


def test_traceback_formatter_gc_detection_with_native():
    # GIVEN

    frame = PyFrame(
        prev=None,
        next=None,
        code=PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        arguments={},
        locals={},
        is_entry=True,
    )
    thread = PyThread(
        tid=1,
        frame=frame,
        native_frames=[
            NativeFrame(0x0, "gc_collect", "native_file1.c", 1, 0, "library.so")
        ],
        holds_the_gil=False,
        is_gc_collecting=-1,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=False, native_last=False))

    # THEN

    assert lines == [
        "Traceback for thread 1 [Garbage collecting] (most recent call last):",
        '    (Python) File "file1.py", line 1, in function1',
        "",
    ]


def test_traceback_formatter_gc_detection_without_native():
    # GIVEN

    frame = PyFrame(
        prev=None,
        next=None,
        code=PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        arguments={},
        locals={},
        is_entry=True,
    )
    thread = PyThread(
        tid=1,
        frame=frame,
        native_frames=[],
        holds_the_gil=True,
        is_gc_collecting=True,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=False, native_last=False))

    # THEN

    assert lines == [
        "Traceback for thread 1 [Has the GIL,Garbage collecting] (most recent call last):",
        '    (Python) File "file1.py", line 1, in function1',
        "",
    ]


def test_traceback_formatter_dropping_the_gil_detection():
    # GIVEN
    frame = PyFrame(
        prev=None,
        next=None,
        code=PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        arguments={},
        locals={},
        is_entry=True,
    )
    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(0x1, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x2, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(0x3, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x4, "native_function3", "native_file3.c", 3, 0, "library.so"),
        NativeFrame(0x5, "drop_gil", "Python/gil.c", 24, 0, "library.so"),
        NativeFrame(0x6, "native_function4", "native_file4.c", 4, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=frame,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=False, native_last=False))

    # THEN

    assert lines == [
        "Traceback for thread 1 [Dropping the GIL] (most recent call last):",
        '    (Python) File "file1.py", line 1, in function1',
        "",
    ]


def test_traceback_formatter_taking_the_gil_detection():
    # GIVEN
    frame = PyFrame(
        prev=None,
        next=None,
        code=PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        arguments={},
        locals={},
        is_entry=True,
    )
    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(0x1, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x2, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(0x3, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x4, "native_function3", "native_file3.c", 3, 0, "library.so"),
        NativeFrame(0x5, "take_gil", "Python/gil.c", 24, 0, "library.so"),
        NativeFrame(0x6, "native_function4", "native_file4.c", 4, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=frame,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=False, native_last=False))

    # THEN

    assert lines == [
        "Traceback for thread 1 [Waiting for the GIL] (most recent call last):",
        '    (Python) File "file1.py", line 1, in function1',
        "",
    ]


def test_traceback_formatter_native_not_matching_simple_eval_frames():
    # GIVEN

    codes = [
        PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        PyCodeObject(
            filename="file2.py",
            scope="function2",
            location=LocationInfo(2, 2, 0, 0),
        ),
        PyCodeObject(
            filename="file3.py",
            scope="function3",
            location=LocationInfo(3, 3, 0, 0),
        ),
    ]

    current_frame = None
    for code in reversed(codes):
        current_frame = PyFrame(
            prev=None,
            next=current_frame,
            code=code,
            arguments={},
            locals={},
            is_entry=True,
        )

    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(0x1, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x2, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(0x3, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x4, "native_function4", "native_file4.c", 4, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=current_frame,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=False, native_last=False))

    # THEN

    assert lines == [
        "Traceback for thread 1 [] (most recent call last):",
        '    (Python) File "file1.py", line 1, in function1',
        '    (Python) File "file2.py", line 2, in function2',
        '    (Python) File "file3.py", line 3, in function3',
        "",
    ]


def test_traceback_formatter_native_not_matching_composite_eval_frames():
    # GIVEN

    codes = [
        PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        PyCodeObject(
            filename="file2.py",
            scope="function2",
            location=LocationInfo(2, 2, 0, 0),
        ),
        PyCodeObject(
            filename="file3.py",
            scope="function3",
            location=LocationInfo(3, 3, 0, 0),
        ),
    ]

    current_frame = None
    for code in reversed(codes):
        current_frame = PyFrame(
            prev=None,
            next=current_frame,
            code=code,
            arguments={},
            locals={},
            is_entry=True,
        )

    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(0x1, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(
            0x2, "PyEval_EvalFrameDefault", "Python/ceval.c", 12, 0, "library.so"
        ),
        NativeFrame(
            0x3, "_PyEval_EvalFrameDefault", "Python/ceval.c", 130, 0, "library.so"
        ),
        NativeFrame(0x4, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(0x5, "native_function3", "native_file3.c", 3, 0, "library.so"),
        NativeFrame(0x6, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(
            0x7, "PyEval_EvalFrameDefault", "Python/ceval.c", 12, 0, "library.so"
        ),
        NativeFrame(
            0x8, "_PyEval_EvalFrameDefault", "Python/ceval.c", 130, 0, "library.so"
        ),
        NativeFrame(0x9, "native_function4", "native_file4.c", 4, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=current_frame,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=False, native_last=False))

    # THEN

    assert lines == [
        "Traceback for thread 1 [] (most recent call last):",
        '    (Python) File "file1.py", line 1, in function1',
        '    (Python) File "file2.py", line 2, in function2',
        '    (Python) File "file3.py", line 3, in function3',
        "",
    ]


def test_traceback_formatter_mixed_inlined_frames():
    # GIVEN

    codes = [
        PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        PyCodeObject(
            filename="file2.py",
            scope="function2",
            location=LocationInfo(2, 2, 0, 0),
        ),
        PyCodeObject(
            filename="file3.py",
            scope="function3",
            location=LocationInfo(3, 3, 0, 0),
        ),
        PyCodeObject(
            filename="file4.py",
            scope="function4",
            location=LocationInfo(4, 4, 0, 0),
        ),
        PyCodeObject(
            filename="file5.py",
            scope="function5",
            location=LocationInfo(5, 5, 0, 0),
        ),
    ]

    current_frame = None
    entry_funcs = {"function1", "function3"}
    for code in reversed(codes):
        current_frame = PyFrame(
            prev=None,
            next=current_frame,
            code=code,
            arguments={},
            locals={},
            is_entry=code.scope in entry_funcs,
        )

    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(0x1, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(
            0x2, "_PyEval_EvalFrameDefault", "Python/ceval.c", 12, 0, "library.so"
        ),
        NativeFrame(0x3, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(0x4, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(
            0x5, "_PyEval_EvalFrameDefault", "Python/ceval.c", 12, 0, "library.so"
        ),
        NativeFrame(0x6, "native_function3", "native_file3.c", 3, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=current_frame,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=True, native_last=False))

    # THEN

    expected_lines = [
        "Traceback for thread 1 [] (most recent call last):",
        '    (C) File "native_file1.c", line 1, in native_function1 (library.so)',
        '    (Python) File "file1.py", line 1, in function1',
        '    (Python) File "file2.py", line 2, in function2',
        '    (C) File "native_file2.c", line 2, in native_function2 (library.so)',
        '    (Python) File "file3.py", line 3, in function3',
        '    (Python) File "file4.py", line 4, in function4',
        '    (Python) File "file5.py", line 5, in function5',
        '    (C) File "native_file3.c", line 3, in native_function3 (library.so)',
        "",
    ]
    assert lines == expected_lines


def test_traceback_formatter_all_inlined_frames():
    # GIVEN

    codes = [
        PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        PyCodeObject(
            filename="file2.py",
            scope="function2",
            location=LocationInfo(2, 2, 0, 0),
        ),
        PyCodeObject(
            filename="file3.py",
            scope="function3",
            location=LocationInfo(3, 3, 0, 0),
        ),
        PyCodeObject(
            filename="file4.py",
            scope="function4",
            location=LocationInfo(4, 4, 0, 0),
        ),
        PyCodeObject(
            filename="file5.py",
            scope="function5",
            location=LocationInfo(5, 5, 0, 0),
        ),
    ]

    current_frame = None
    for code in reversed(codes):
        current_frame = PyFrame(
            prev=None,
            next=current_frame,
            code=code,
            arguments={},
            locals={},
            is_entry=False,
        )
    current_frame.is_entry = True

    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(0x1, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(
            0x2, "_PyEval_EvalFrameDefault", "Python/ceval.c", 12, 0, "library.so"
        ),
        NativeFrame(0x6, "native_function3", "native_file3.c", 3, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=current_frame,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=True, native_last=False))

    # THEN

    expected_lines = [
        "Traceback for thread 1 [] (most recent call last):",
        '    (C) File "native_file1.c", line 1, in native_function1 (library.so)',
        '    (Python) File "file1.py", line 1, in function1',
        '    (Python) File "file2.py", line 2, in function2',
        '    (Python) File "file3.py", line 3, in function3',
        '    (Python) File "file4.py", line 4, in function4',
        '    (Python) File "file5.py", line 5, in function5',
        '    (C) File "native_file3.c", line 3, in native_function3 (library.so)',
        "",
    ]
    assert lines == expected_lines


def test_traceback_formatter_native_last():
    # GIVEN

    codes = [
        PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        PyCodeObject(
            filename="file2.py",
            scope="function2",
            location=LocationInfo(2, 2, 0, 0),
        ),
        PyCodeObject(
            filename="file3.py",
            scope="function3",
            location=LocationInfo(3, 3, 0, 0),
        ),
        PyCodeObject(
            filename="file4.py",
            scope="function4",
            location=LocationInfo(4, 4, 0, 0),
        ),
        PyCodeObject(
            filename="file5.py",
            scope="function5",
            location=LocationInfo(5, 5, 0, 0),
        ),
    ]

    current_frame = None
    for code in reversed(codes):
        current_frame = PyFrame(
            prev=None,
            next=current_frame,
            code=code,
            arguments={},
            locals={},
            is_entry=False,
        )
    current_frame.is_entry = True

    native_frames = [
        NativeFrame(0x0, "native_function1", "native_file1.c", 1, 0, "library.so"),
        NativeFrame(0x1, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"),
        NativeFrame(0x3, "native_function2", "native_file2.c", 2, 0, "library.so"),
        NativeFrame(
            0x2, "_PyEval_EvalFrameDefault", "Python/ceval.c", 12, 0, "library.so"
        ),
        NativeFrame(0x4, "native_function3", "native_file3.c", 3, 0, "library.so"),
        NativeFrame(0x6, "native_function4", "native_file4.c", 4, 0, "library.so"),
    ]

    thread = PyThread(
        tid=1,
        frame=current_frame,
        native_frames=native_frames,
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    lines = list(format_thread(thread, native=True, native_last=True))

    # THEN

    expected_lines = [
        "Traceback for thread 1 [] (most recent call last):",
        '    (Python) File "file1.py", line 1, in function1',
        '    (Python) File "file2.py", line 2, in function2',
        '    (Python) File "file3.py", line 3, in function3',
        '    (Python) File "file4.py", line 4, in function4',
        '    (Python) File "file5.py", line 5, in function5',
        '    (C) File "native_file3.c", line 3, in native_function3 (library.so)',
        '    (C) File "native_file4.c", line 4, in native_function4 (library.so)',
        "",
    ]
    assert lines == expected_lines


def test_print_thread(capsys):
    # GIVEN
    thread = PyThread(
        tid=1,
        frame=None,
        native_frames=[],
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )
    # WHEN

    with patch(
        "pystack.traceback_formatter.format_thread",
        return_value=("1", "2", "3"),
    ):
        print_thread(thread, native=False, native_last=False)

    # THEN

    captured = capsys.readouterr()

    assert captured.out == "1\n2\n3\n"
    assert captured.err == ""


@pytest.mark.parametrize("location_info", [(1, 0, 1, 0), (1, 1, 0, 0)])
@pytest.mark.parametrize(
    "arguments, locals, expected_locals_render",
    [
        (
            {"the_argument": "some_value", "the_second_argument": "42"},
            {"the_local": "some_other_value", "the_second_local": "7"},
            [
                "      Arguments:",
                "        the_argument: some_value",
                "        the_second_argument: 42",
                "      Locals:",
                "        the_local: some_other_value",
                "        the_second_local: 7",
            ],
        ),
        (
            {},
            {"the_local": "some_other_value"},
            [
                "      Locals:",
                "        the_local: some_other_value",
            ],
        ),
        (
            {"the_argument": "some_value"},
            {},
            [
                "      Arguments:",
                "        the_argument: some_value",
            ],
        ),
        (
            {"the_argument": "\x1b[6;30;42m some_value\nwith\nnewlines'\x1b[0m"},
            {},
            [
                "      Arguments:",
                "        the_argument: \\x1b[6;30;42m some_value\\nwith\\nnewlines'\\x1b[0m",
            ],
        ),
    ],
)
def test_traceback_formatter_locals(
    arguments, locals, location_info, expected_locals_render
):
    # GIVEN
    frame = PyFrame(
        prev=None,
        next=None,
        code=PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(*location_info),
        ),
        arguments=arguments,
        locals=locals,
        is_entry=True,
    )

    thread = PyThread(
        tid=1,
        frame=frame,
        native_frames=[],
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN

    source_data = "\n".join(
        f'x = "This is the line {line}" or (1+1)' for line in range(1, 5)
    )
    with patch("builtins.open", mock_open(read_data=source_data)), patch(
        "os.path.exists", return_value=True
    ):
        lines = list(format_thread(thread, native=False, native_last=False))

    # THEN
    print(lines)
    assert lines == [
        "Traceback for thread 1 [] (most recent call last):",
        '    (Python) File "file1.py", line 1, in function1',
        '        x = "This is the line 1" or (1+1)',
    ] + expected_locals_render + [""]


def test_traceback_formatter_thread_names():
    # GIVEN
    frame = PyFrame(
        prev=None,
        next=None,
        code=PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 0),
        ),
        arguments=[],
        locals=[],
        is_entry=True,
    )

    thread = PyThread(
        tid=1,
        frame=frame,
        native_frames=[],
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
        name="foo",
    )

    # WHEN

    lines = list(format_thread(thread, native=False, native_last=False))

    # THEN
    print(lines)
    assert lines == [
        "Traceback for thread 1 (foo) [] (most recent call last):",
        '    (Python) File "file1.py", line 1, in function1',
    ] + [""]


def test_traceback_formatter_position_infomation():
    # GIVEN

    codes = [
        PyCodeObject(
            filename="file1.py",
            scope="function1",
            location=LocationInfo(1, 1, 0, 3),
        ),
        PyCodeObject(
            filename="file2.py",
            scope="function2",
            location=LocationInfo(2, 2, 4, 25),
        ),
        PyCodeObject(
            filename="file3.py",
            scope="function3",
            location=LocationInfo(3, 3, 28, 33),
        ),
    ]

    current_frame = None
    for code in reversed(codes):
        current_frame = PyFrame(
            prev=None,
            next=current_frame,
            code=code,
            arguments={},
            locals={},
            is_entry=True,
        )

    thread = PyThread(
        tid=1,
        frame=current_frame,
        native_frames=[],
        holds_the_gil=False,
        is_gc_collecting=False,
        python_version=(3, 8),
    )

    # WHEN
    source_data = "\n".join(
        f'x = "This is the line {line}" or (1+1)' for line in range(1, 5)
    )
    with patch("builtins.open", mock_open(read_data=source_data)), patch(
        "os.path.exists", return_value=True
    ), patch(
        "pystack.traceback_formatter.colored",
        side_effect=lambda x, *args, **kwargs: x,
    ) as colored_mock:
        lines = list(format_thread(thread, native=False, native_last=False))

    # THEN

    assert lines == [
        "Traceback for thread 1 [] (most recent call last):",
        '    (Python) File "file1.py", line 1, in function1',
        '        x = "This is the line 1" or (1+1)',
        '    (Python) File "file2.py", line 2, in function2',
        '        x = "This is the line 2" or (1+1)',
        '    (Python) File "file3.py", line 3, in function3',
        '        x = "This is the line 3" or (1+1)',
        "",
    ]
    colored_mock.assert_any_call("x =", color="blue")
    colored_mock.assert_any_call('"This is the line 2" ', color="blue")
    colored_mock.assert_any_call("(1+1)", color="blue")
