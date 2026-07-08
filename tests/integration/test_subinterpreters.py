import io
from collections import Counter
from contextlib import redirect_stdout
from pathlib import Path
from typing import Dict
from typing import List
from typing import Set

import pytest

from pystack.engine import NativeReportingMode
from pystack.engine import StackMethod
from pystack.engine import get_process_threads
from pystack.engine import get_process_threads_for_core
from pystack.traceback_formatter import print_threads
from pystack.types import NativeFrame
from pystack.types import frame_type
from tests.utils import ALL_PYTHONS_THAT_SUPPORT_SUBINTERPRETERS
from tests.utils import generate_core_file
from tests.utils import spawn_child_process

NUM_INTERPRETERS = 3
NUM_INTERPRETERS_WITH_THREADS = 2
NUM_THREADS_PER_SUBINTERPRETER = 2

SCRIPTS_DIR = Path(__file__).parent
SUBINTERPRETERS_SHIM = SCRIPTS_DIR / "subinterpreters_shim.py"
TEST_SUBINTERPRETERS = SCRIPTS_DIR / "subinterpreters_program.py"
TEST_SUBINTERPRETERS_WITH_THREADS = (
    SCRIPTS_DIR / "subinterpreters_with_threads_program.py"
)
TEST_SUBINTERPRETERS_NESTED_SAME_THREAD = (
    SCRIPTS_DIR / "subinterpreters_nested_same_thread_program.py"
)
TEST_SUBINTERPRETERS_TWO_CHAINS = SCRIPTS_DIR / "subinterpreters_two_chains_program.py"


def _collect_threads(
    python_executable: Path,
    tmpdir: Path,
    native_mode: NativeReportingMode = NativeReportingMode.OFF,
    test_file: Path = TEST_SUBINTERPRETERS,
):
    script = tmpdir / "subinterpreters_program.py"
    script.write_bytes(test_file.read_bytes())
    (tmpdir / "subinterpreters_shim.py").write_bytes(SUBINTERPRETERS_SHIM.read_bytes())

    with spawn_child_process(
        str(python_executable), str(script), tmpdir
    ) as child_process:
        return list(
            get_process_threads(
                child_process.pid,
                stop_process=True,
                native_mode=native_mode,
                method=StackMethod.AUTO,
            )
        )


def _assert_interpreter_headers(
    threads,
    native_mode: NativeReportingMode,
    interpreter_ids,
) -> str:
    output = io.StringIO()
    with redirect_stdout(output):
        print_threads(threads, native_mode=native_mode)

    result = output.getvalue()
    assert "In the main interpreter" in result
    for interpreter_id in interpreter_ids:
        if interpreter_id == 0:
            continue
        assert f"In interpreter {interpreter_id}" in result
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
    groups: Dict[int, List] = {}
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
def test_subinterpreters(python, tmp_path):
    _, python_executable = python

    threads = _collect_threads(
        python_executable=python_executable,
        tmpdir=tmp_path,
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
def test_subinterpreters_with_native(python, tmp_path, native_mode):
    _, python_executable = python

    threads = _collect_threads(
        python_executable=python_executable,
        tmpdir=tmp_path,
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
def test_subinterpreters_many_threads_with_native(python, tmp_path):
    _, python_executable = python

    threads = _collect_threads(
        python_executable=python_executable,
        tmpdir=tmp_path,
        native_mode=NativeReportingMode.PYTHON,
        test_file=TEST_SUBINTERPRETERS_WITH_THREADS,
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
def test_subinterpreters_nested_same_thread_with_native(python, tmp_path):
    _, python_executable = python

    threads = _collect_threads(
        python_executable=python_executable,
        tmpdir=tmp_path,
        native_mode=NativeReportingMode.PYTHON,
        test_file=TEST_SUBINTERPRETERS_NESTED_SAME_THREAD,
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
def test_subinterpreters_two_threads_three_per_thread_with_native(python, tmp_path):
    _, python_executable = python

    threads = _collect_threads(
        python_executable=python_executable,
        tmpdir=tmp_path,
        native_mode=NativeReportingMode.PYTHON,
        test_file=TEST_SUBINTERPRETERS_TWO_CHAINS,
    )

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
def test_subinterpreters_for_core(python, tmp_path):
    _, python_executable = python

    with generate_core_file(
        python_executable, TEST_SUBINTERPRETERS, tmp_path
    ) as core_file:
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
def test_subinterpreters_for_core_with_native(python, tmp_path, native_mode):
    _, python_executable = python

    with generate_core_file(
        python_executable, TEST_SUBINTERPRETERS, tmp_path
    ) as core_file:
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
