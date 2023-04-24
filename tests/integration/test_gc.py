import re
from pathlib import Path

from pystack.engine import NativeReportingMode
from pystack.engine import get_process_threads
from pystack.engine import get_process_threads_for_core
from tests.utils import ALL_PYTHONS
from tests.utils import ALL_PYTHONS_WITH_SYMBOLS
from tests.utils import generate_core_file
from tests.utils import spawn_child_process

TEST_GC = Path(__file__).parent / "gc_freeze_program.py"
TEST_SINGLE_THREAD_FILE = Path(__file__).parent / "single_thread_program.py"

VERSION_REGEXP = re.compile(r"python(?P<major>\d+)\.(?P<minor>\d+)")


@ALL_PYTHONS_WITH_SYMBOLS
def test_gc_status_is_reported_when_garbage_collecting_in_process(python, tmpdir):
    # GIVEN

    _, python_executable = python

    # WHEN

    with spawn_child_process(python_executable, TEST_GC, tmpdir) as child_process:
        threads = list(
            get_process_threads(
                child_process.pid,
                stop_process=True,
                native_mode=NativeReportingMode.PYTHON,
            )
        )

    # THEN

    assert len(threads) == 2
    assert {thread.gc_status for thread in threads} == {"", "Garbage collecting"}


@ALL_PYTHONS
def test_gc_status_is_reported_when_no_garbage_collecting_in_process(python, tmpdir):
    # GIVEN

    _, python_executable = python

    # WHEN

    with spawn_child_process(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        threads = list(
            get_process_threads(
                child_process.pid,
                stop_process=True,
                native_mode=NativeReportingMode.PYTHON,
            )
        )

    # THEN

    assert len(threads) == 1
    assert {thread.gc_status for thread in threads} == {""}


@ALL_PYTHONS_WITH_SYMBOLS
def test_gc_status_is_reported_when_garbage_collecting_in_core(python, tmpdir):
    # GIVEN

    _, python_executable = python

    # WHEN

    with generate_core_file(python_executable, TEST_GC, tmpdir) as core_file:
        threads = list(
            get_process_threads_for_core(
                core_file,
                Path(python_executable),
                native_mode=NativeReportingMode.PYTHON,
            )
        )

    # THEN

    assert len(threads) == 2
    assert {thread.gc_status for thread in threads} == {"", "Garbage collecting"}


@ALL_PYTHONS
def test_gc_status_is_reported_when_no_garbage_collecting_in_core(python, tmpdir):
    # GIVEN

    _, python_executable = python

    # WHEN

    with generate_core_file(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as core_file:
        threads = list(
            get_process_threads_for_core(
                core_file,
                Path(python_executable),
                native_mode=NativeReportingMode.PYTHON,
            )
        )

    # THEN

    assert len(threads) == 1
    assert {thread.gc_status for thread in threads} == {""}


@ALL_PYTHONS
def test_gc_status_is_reported_when_garbage_collecting_in_process_no_native(
    python, tmpdir
):
    # GIVEN

    (major_version, minor_version), python_executable = python

    # WHEN

    with spawn_child_process(python_executable, TEST_GC, tmpdir) as child_process:
        threads = list(
            get_process_threads(
                child_process.pid,
                stop_process=True,
            )
        )

    # THEN

    assert len(threads) == 2
    if int(major_version) < 3 or int(minor_version) < 7:
        assert {thread.is_gc_collecting for thread in threads} == {-1}
    else:
        assert {thread.is_gc_collecting for thread in threads} == {1}


@ALL_PYTHONS
def test_gc_status_is_reported_when_garbage_collecting_in_core_no_native(
    python, tmpdir
):
    # GIVEN

    (major_version, minor_version), python_executable = python

    # WHEN

    with generate_core_file(python_executable, TEST_GC, tmpdir) as core_file:
        threads = list(get_process_threads_for_core(core_file, Path(python_executable)))

    # THEN

    assert len(threads) == 2
    if int(major_version) < 3 or int(minor_version) < 7:
        assert {thread.is_gc_collecting for thread in threads} == {-1}
    else:
        assert {thread.is_gc_collecting for thread in threads} == {1}
