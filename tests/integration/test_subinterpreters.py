import io
import subprocess
import time
from collections import Counter
from contextlib import redirect_stdout
from pathlib import Path
from typing import Set

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

# Compatibility shim so test programs work on both 3.13 (_interpreters)
# and 3.14+ (concurrent.interpreters).
_INTERPRETERS_SHIM = """\
import sys as _sys
try:
    from concurrent import interpreters
except ImportError:
    import _interpreters as _raw
    class _W:
        def __init__(self, id):
            self.id = id
        def exec(self, code):
            _raw.exec(self.id, code)
    class interpreters:
        @staticmethod
        def create():
            return _W(_raw.create())
        Interpreter = _W
"""

PROGRAM = f"""\
import sys
import threading
import time

{_INTERPRETERS_SHIM}

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

{_INTERPRETERS_SHIM}

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

PROGRAM_NESTED_SAME_THREAD = (
    """\
import sys
import threading
import time

"""
    + _INTERPRETERS_SHIM
    + """
_SHIM = '''"""
    + _INTERPRETERS_SHIM
    + """'''

fifo = sys.argv[1]

interp_outer = interpreters.create()
interp_inner = interpreters.create()

inner_code = f'''\\
import time
with open({fifo!r}, "w") as f:
    f.write("ready")
while True:
    time.sleep(1)
'''
outer_code = _SHIM + f'''
interpreters.Interpreter({{inner_id}}).exec({{inner_code!r}})
'''.format(inner_id=interp_inner.id, inner_code=inner_code)

t = threading.Thread(target=interp_outer.exec, args=(outer_code,))
t.daemon = True
t.start()

while True:
    time.sleep(1)
"""
)

PROGRAM_TWO_THREADS_THREE_SUBINTERPRETERS_EACH = (
    """\
import sys
import threading
import time
from pathlib import Path

"""
    + _INTERPRETERS_SHIM
    + """
_SHIM = '''"""
    + _INTERPRETERS_SHIM
    + """'''

signal_file = Path(sys.argv[1])


def make_level3_code(token):
    return f'''\\
import time
from pathlib import Path
Path({str(signal_file)!r}).open("a").write("{token}\\\\n")
while True:
    time.sleep(1)
'''


def make_level2_code(interp3_id, level3_code):
    return _SHIM + f'''
interpreters.Interpreter({interp3_id}).exec({level3_code!r})
'''


def make_level1_code(interp2_id, level2_code):
    return _SHIM + f'''
interpreters.Interpreter({interp2_id}).exec({level2_code!r})
'''


def launch_chain(token):
    interp1 = interpreters.create()
    interp2 = interpreters.create()
    interp3 = interpreters.create()

    level3_code = make_level3_code(token)
    level2_code = make_level2_code(interp3.id, level3_code)
    level1_code = make_level1_code(interp2.id, level2_code)
    interp1.exec(level1_code)


t1 = threading.Thread(target=launch_chain, args=("chain1",), daemon=True)
t2 = threading.Thread(target=launch_chain, args=("chain2",), daemon=True)
t1.start()
t2.start()

while True:
    time.sleep(1)
"""
)


def _collect_threads(
    python_executable: Path,
    tmpdir: Path,
    native_mode: NativeReportingMode = NativeReportingMode.OFF,
):
    test_file = Path(str(tmpdir)) / "subinterpreters_program.py"
    test_file.write_text(PROGRAM)

    with spawn_child_process(
        str(python_executable), str(test_file), tmpdir
    ) as child_process:
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


def _interpreter_ids(threads) -> Set[int]:
    return {
        thread.interpreter_id for thread in threads if thread.interpreter_id is not None
    }


def _assert_subinterpreter_coverage(threads) -> Set[int]:
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


def _assert_mergeable_same_tid_groups(threads) -> bool:
    groups = {}
    for thread in threads:
        groups.setdefault(thread.tid, []).append(thread)

    found_shared_tid = False
    for group in groups.values():
        interpreter_ids = {
            thread.interpreter_id
            for thread in group
            if thread.interpreter_id is not None
        }
        if len(group) < 2 or len(interpreter_ids) < 2:
            continue
        found_shared_tid = True
        for thread in group:
            eval_frames = [
                frame
                for frame in thread.native_frames
                if frame_type(frame, thread.python_version)
                == NativeFrame.FrameType.EVAL
            ]
            entry_count = sum(frame.is_entry for frame in thread.all_frames)
            assert len(eval_frames) == entry_count
    return found_shared_tid


def _shared_tid_groups_with_min_interpreters(threads, min_interpreters):
    groups = {}
    for thread in threads:
        groups.setdefault(thread.tid, []).append(thread)

    matching = []
    for tid, group in groups.items():
        interpreter_ids = {
            thread.interpreter_id
            for thread in group
            if thread.interpreter_id is not None
        }
        if len(interpreter_ids) >= min_interpreters:
            matching.append((tid, group))
    return matching


def _assert_strict_native_eval_symbols_for_group(group) -> None:
    for thread in group:
        eval_frames = [
            frame
            for frame in thread.native_frames
            if frame_type(frame, thread.python_version) == NativeFrame.FrameType.EVAL
        ]
        assert eval_frames
        assert all("?" not in frame.symbol for frame in eval_frames)
        if any(frame.linenumber == 0 for frame in eval_frames):
            assert all(frame.linenumber == 0 for frame in eval_frames)
            assert all(frame.path == "???" for frame in eval_frames)
        else:
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
def test_subinterpreters_nested_same_thread_with_native(python, tmpdir):
    _, python_executable = python

    test_file = Path(str(tmpdir)) / "subinterpreters_nested_same_thread.py"
    test_file.write_text(PROGRAM_NESTED_SAME_THREAD)

    with spawn_child_process(python_executable, test_file, tmpdir) as child_process:
        threads = list(
            get_process_threads(
                child_process.pid,
                stop_process=True,
                native_mode=NativeReportingMode.PYTHON,
                method=StackMethod.DEBUG_OFFSETS,
            )
        )

    assert any(thread.native_frames for thread in threads)
    _assert_native_eval_symbols(threads)

    has_shared_tid = _assert_mergeable_same_tid_groups(threads)
    assert has_shared_tid

    output = _assert_interpreter_headers(
        threads=threads,
        native_mode=NativeReportingMode.PYTHON,
        interpreter_ids=_interpreter_ids(threads),
    )
    assert (
        "Unable to merge native stack due to insufficient native information"
        not in output
    )


@ALL_PYTHONS_THAT_SUPPORT_SUBINTERPRETERS
def test_subinterpreters_two_threads_three_per_thread_with_native(python, tmpdir):
    _, python_executable = python

    test_file = Path(str(tmpdir)) / "subinterpreters_two_threads_three_each.py"
    signal_file = Path(str(tmpdir)) / "subinterpreters_ready.txt"
    signal_file.write_text("")
    test_file.write_text(PROGRAM_TWO_THREADS_THREE_SUBINTERPRETERS_EACH)

    with subprocess.Popen(
        [str(python_executable), str(test_file), str(signal_file)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    ) as child_process:
        deadline = time.time() + 10
        while time.time() < deadline:
            lines = [line for line in signal_file.read_text().splitlines() if line]
            if len(lines) >= 2:
                break
            time.sleep(0.1)
        else:
            child_process.terminate()
            child_process.kill()
            raise AssertionError("Timed out waiting for nested subinterpreter chains")

        threads = list(
            get_process_threads(
                child_process.pid,
                stop_process=True,
                native_mode=NativeReportingMode.PYTHON,
                method=StackMethod.DEBUG_OFFSETS,
            )
        )

        child_process.terminate()
        child_process.kill()
        child_process.wait(timeout=5)

    groups = _shared_tid_groups_with_min_interpreters(threads, min_interpreters=3)
    assert len(groups) >= 2

    for _, group in groups:
        _assert_strict_native_eval_symbols_for_group(group)
        for thread in group:
            eval_frames = [
                frame
                for frame in thread.native_frames
                if frame_type(frame, thread.python_version)
                == NativeFrame.FrameType.EVAL
            ]
            entry_count = sum(frame.is_entry for frame in thread.all_frames)
            assert len(eval_frames) == entry_count
            assert len(eval_frames) > 0

    output = _assert_interpreter_headers(
        threads=threads,
        native_mode=NativeReportingMode.PYTHON,
        interpreter_ids=_interpreter_ids(threads),
    )
    assert (
        "Unable to merge native stack due to insufficient native information"
        not in output
    )


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
