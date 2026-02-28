import io
from collections import Counter
from contextlib import redirect_stdout
from pathlib import Path

import pytest

from pystack.engine import NativeReportingMode
from pystack.engine import StackMethod
from pystack.engine import get_process_threads
from pystack.engine import get_process_threads_for_core
from pystack.traceback_formatter import TracebackPrinter
from pystack.types import NativeFrame
from pystack.types import frame_type
from tests.utils import ALL_PYTHONS_THAT_SUPPORT_SUBINTERPRETERS
from tests.utils import generate_core_file
from tests.utils import spawn_child_process

NUM_INTERPRETERS = 3
NUM_INTERPRETERS_WITH_THREADS = 2
NUM_THREADS_PER_SUBINTERPRETER = 2

PROGRAM = f"""\
import sys
import threading
import time

from concurrent import interpreters

NUM_INTERPRETERS = {NUM_INTERPRETERS}


def start_interpreter_async(interp, code):
    t = threading.Thread(target=interp.exec, args=(code,))
    t.daemon = True
    t.start()
    return t


CODE = '''\\
import time
while True:
    time.sleep(1)
'''

threads = []
for _ in range(NUM_INTERPRETERS):
    interp = interpreters.create()
    t = start_interpreter_async(interp, CODE)
    threads.append(t)

# Give sub-interpreters time to start executing
time.sleep(1)

fifo = sys.argv[1]
with open(fifo, "w") as f:
    f.write("ready")

while True:
    time.sleep(1)
"""


PROGRAM_WITH_THREADS = f"""\
import sys
import threading
import time

from concurrent import interpreters

NUM_INTERPRETERS = {NUM_INTERPRETERS_WITH_THREADS}


def start_interpreter_async(interp, code):
    t = threading.Thread(target=interp.exec, args=(code,))
    t.daemon = True
    t.start()
    return t


CODE = '''\\
import threading
import time

NUM_THREADS = {NUM_THREADS_PER_SUBINTERPRETER}

def worker():
    while True:
        time.sleep(1)

threads = []
for _ in range(NUM_THREADS):
    t = threading.Thread(target=worker)
    # daemon threads are disabled in isolated subinterpreters
    t.start()
    threads.append(t)

while True:
    time.sleep(1)
'''

threads = []
for _ in range(NUM_INTERPRETERS):
    interp = interpreters.create()
    t = start_interpreter_async(interp, CODE)
    threads.append(t)

# Give sub-interpreters and their internal workers time to start.
time.sleep(2)

fifo = sys.argv[1]
with open(fifo, "w") as f:
    f.write("ready")

while True:
    time.sleep(1)
"""


def _collect_threads(
    python_executable: Path,
    tmpdir: Path,
    native_mode: NativeReportingMode = NativeReportingMode.OFF,
):
    test_file = Path(str(tmpdir)) / "subinterpreters_program.py"
    test_file.write_text(PROGRAM)

    with spawn_child_process(python_executable, test_file, tmpdir) as child_process:
        return list(
            get_process_threads(
                child_process.pid,
                stop_process=True,
                native_mode=native_mode,
            )
        )


def _assert_interpreter_headers(
    threads,
    native_mode: NativeReportingMode,
    interpreter_ids,
) -> str:
    printer = TracebackPrinter(
        native_mode=native_mode,
        include_subinterpreters=True,
    )
    output = io.StringIO()
    with redirect_stdout(output):
        for thread in threads:
            printer.print_thread(thread)

    result = output.getvalue()
    assert "Interpreter-0 (main)" in result
    for interpreter_id in interpreter_ids:
        if interpreter_id == 0:
            continue
        assert f"Interpreter-{interpreter_id}" in result
    return result


def _count_threads_by_interpreter(threads):
    return dict(
        Counter(
            thread.interpreter_id
            for thread in threads
            if thread.interpreter_id is not None
        )
    )


def _interpreter_ids(threads) -> set[int]:
    return {
        thread.interpreter_id for thread in threads if thread.interpreter_id is not None
    }


def _assert_subinterpreter_coverage(threads) -> set[int]:
    interpreter_ids = _interpreter_ids(threads)
    assert 0 in interpreter_ids
    assert len(interpreter_ids) == NUM_INTERPRETERS + 1
    return interpreter_ids


def _assert_native_eval_symbols(threads) -> None:
    eval_frames = [
        frame
        for thread in threads
        for frame in thread.native_frames
        if frame_type(frame, thread.python_version) == NativeFrame.FrameType.EVAL
    ]
    assert eval_frames
    assert all("?" not in frame.symbol for frame in eval_frames)
    if any(frame.linenumber == 0 for frame in eval_frames):  # pragma: no cover
        assert all(frame.linenumber == 0 for frame in eval_frames)
        assert all(frame.path == "???" for frame in eval_frames)
    else:  # pragma: no cover
        assert all(frame.linenumber != 0 for frame in eval_frames)
        assert any(frame.path and "?" not in frame.path for frame in eval_frames)


@ALL_PYTHONS_THAT_SUPPORT_SUBINTERPRETERS
def test_subinterpreters(python, tmpdir):
    _, python_executable = python

    threads = _collect_threads(
        python_executable=python_executable,
        tmpdir=tmpdir,
        native_mode=NativeReportingMode.OFF,
    )

    interpreter_ids = _assert_subinterpreter_coverage(threads)
    assert all(not thread.native_frames for thread in threads)
    _assert_interpreter_headers(
        threads=threads,
        native_mode=NativeReportingMode.OFF,
        interpreter_ids=interpreter_ids,
    )


@ALL_PYTHONS_THAT_SUPPORT_SUBINTERPRETERS
@pytest.mark.parametrize(
    "native_mode",
    [
        NativeReportingMode.PYTHON,
        NativeReportingMode.LAST,
        NativeReportingMode.ALL,
    ],
    ids=["python", "last", "all"],
)
def test_subinterpreters_with_native(python, tmpdir, native_mode):
    _, python_executable = python

    threads = _collect_threads(
        python_executable=python_executable,
        tmpdir=tmpdir,
        native_mode=native_mode,
    )

    interpreter_ids = _assert_subinterpreter_coverage(threads)
    assert any(thread.native_frames for thread in threads)
    _assert_native_eval_symbols(threads)

    output = _assert_interpreter_headers(
        threads=threads,
        native_mode=native_mode,
        interpreter_ids=interpreter_ids,
    )
    assert "(C)" in output or "Unable to merge native stack" in output


@ALL_PYTHONS_THAT_SUPPORT_SUBINTERPRETERS
def test_subinterpreters_many_threads_with_native(python, tmpdir):
    _, python_executable = python

    test_file = Path(str(tmpdir)) / "subinterpreters_with_threads_program.py"
    test_file.write_text(PROGRAM_WITH_THREADS)

    with spawn_child_process(python_executable, test_file, tmpdir) as child_process:
        threads = list(
            get_process_threads(
                child_process.pid,
                stop_process=True,
                native_mode=NativeReportingMode.PYTHON,
                method=StackMethod.DEBUG_OFFSETS,
            )
        )

    interpreter_ids = _interpreter_ids(threads)
    assert 0 in interpreter_ids
    assert len(interpreter_ids) == NUM_INTERPRETERS_WITH_THREADS + 1

    counts_by_interpreter = _count_threads_by_interpreter(threads)
    assert all(
        counts_by_interpreter.get(interpreter_id, 0) >= 1
        for interpreter_id in interpreter_ids
    )
    # At least one sub-interpreter should expose multiple Python threads.
    assert any(
        count > 1
        for interpreter_id, count in counts_by_interpreter.items()
        if interpreter_id != 0
    )

    assert any(thread.native_frames for thread in threads)
    _assert_native_eval_symbols(threads)


@ALL_PYTHONS_THAT_SUPPORT_SUBINTERPRETERS
def test_subinterpreters_for_core(python, tmpdir):
    _, python_executable = python

    test_file = Path(str(tmpdir)) / "subinterpreters_program.py"
    test_file.write_text(PROGRAM)

    with generate_core_file(python_executable, test_file, tmpdir) as core_file:
        threads = list(
            get_process_threads_for_core(
                core_file,
                python_executable,
                native_mode=NativeReportingMode.OFF,
            )
        )

    interpreter_ids = _assert_subinterpreter_coverage(threads)
    assert all(not thread.native_frames for thread in threads)
    _assert_interpreter_headers(
        threads=threads,
        native_mode=NativeReportingMode.OFF,
        interpreter_ids=interpreter_ids,
    )


@ALL_PYTHONS_THAT_SUPPORT_SUBINTERPRETERS
@pytest.mark.parametrize(
    "native_mode",
    [
        NativeReportingMode.PYTHON,
        NativeReportingMode.LAST,
        NativeReportingMode.ALL,
    ],
    ids=["python", "last", "all"],
)
def test_subinterpreters_for_core_with_native(python, tmpdir, native_mode):
    _, python_executable = python

    test_file = Path(str(tmpdir)) / "subinterpreters_program.py"
    test_file.write_text(PROGRAM)

    with generate_core_file(python_executable, test_file, tmpdir) as core_file:
        threads = list(
            get_process_threads_for_core(
                core_file,
                python_executable,
                native_mode=native_mode,
            )
        )

    interpreter_ids = _assert_subinterpreter_coverage(threads)
    assert any(thread.native_frames for thread in threads)
    _assert_native_eval_symbols(threads)
    output = _assert_interpreter_headers(
        threads=threads,
        native_mode=native_mode,
        interpreter_ids=interpreter_ids,
    )
    assert "(C)" in output or "Unable to merge native stack" in output
