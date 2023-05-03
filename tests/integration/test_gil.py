from pathlib import Path

from pystack.engine import get_process_threads
from pystack.engine import get_process_threads_for_core
from tests.utils import ALL_PYTHONS
from tests.utils import generate_core_file
from tests.utils import spawn_child_process

TEST_MULTIPLE_THREADS_GIL_FILE = (
    Path(__file__).parent / "multiple_thread_program_gil.py"
)
TEST_MULTIPLE_THREADS_FILE = Path(__file__).parent / "multiple_thread_program.py"
TEST_SINGLE_THREAD_GIL_FILE = Path(__file__).parent / "single_thread_program_gil.py"
TEST_SINGLE_THREAD_FILE = Path(__file__).parent / "single_thread_program.py"


@ALL_PYTHONS
def test_gil_status_one_thread_among_many_holds_the_gil(python, tmpdir):
    # GIVEN
    _, python_executable = python

    # WHEN

    with spawn_child_process(
        python_executable, TEST_MULTIPLE_THREADS_GIL_FILE, tmpdir
    ) as child_process:
        threads = list(get_process_threads(child_process.pid, stop_process=True))

    # THEN

    assert len(threads) == 4
    assert sorted(thread.holds_the_gil for thread in threads) == [0, 0, 0, 1]


@ALL_PYTHONS
def test_gil_status_no_thread_among_many_holds_the_gil(python, tmpdir):
    # GIVEN
    _, python_executable = python

    # WHEN

    with spawn_child_process(
        python_executable, TEST_MULTIPLE_THREADS_FILE, tmpdir
    ) as child_process:
        threads = list(get_process_threads(child_process.pid, stop_process=True))

    # THEN

    assert len(threads) == 4
    nogil_threads = [thread for thread in threads if not thread.holds_the_gil]
    assert len(nogil_threads) == 4


@ALL_PYTHONS
def test_gil_status_single_thread_holds_the_gil(python, tmpdir):
    # GIVEN
    _, python_executable = python

    # WHEN

    with spawn_child_process(
        python_executable, TEST_SINGLE_THREAD_GIL_FILE, tmpdir
    ) as child_process:
        threads = list(get_process_threads(child_process.pid, stop_process=True))

    # THEN

    assert len(threads) == 1
    (thread,) = threads
    assert thread.holds_the_gil


@ALL_PYTHONS
def test_gil_status_single_thread_does_not_hold_the_gil(python, tmpdir):
    # GIVEN
    _, python_executable = python

    # WHEN

    with spawn_child_process(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        threads = list(get_process_threads(child_process.pid, stop_process=True))

    # THEN

    assert len(threads) == 1
    (thread,) = threads
    assert not thread.holds_the_gil


@ALL_PYTHONS
def test_gil_status_one_thread_among_many_holds_the_gil_for_core(python, tmpdir):
    """Generate a core file for a process with multiple threads in which we know
    one of them holds the GIL at the time the core was generated and check that we
    can detect which thread holds the GIL.
    """
    # GIVEN
    _, python_executable = python

    # WHEN

    with generate_core_file(
        python_executable, TEST_MULTIPLE_THREADS_GIL_FILE, tmpdir
    ) as core_file:
        threads = list(get_process_threads_for_core(core_file, Path(python_executable)))

    # THEN

    assert len(threads) == 4
    assert sorted(thread.holds_the_gil for thread in threads) == [0, 0, 0, 1]


@ALL_PYTHONS
def test_gil_status_no_thread_among_many_holds_the_gil_for_core(python, tmpdir):
    """Generate a core file for a process with multiple threads in which we know
    none of them holds the GIL at the time the core was generated and check that we
    can detect that no thread holds the GIL.
    """
    # GIVEN
    _, python_executable = python

    # WHEN

    with generate_core_file(
        python_executable, TEST_MULTIPLE_THREADS_FILE, tmpdir
    ) as core_file:
        threads = list(get_process_threads_for_core(core_file, Path(python_executable)))

    # THEN

    assert len(threads) == 4
    nogil_threads = [thread for thread in threads if not thread.holds_the_gil]
    assert len(nogil_threads) == 4


@ALL_PYTHONS
def test_gil_status_single_thread_holds_the_gil_for_core(python, tmpdir):
    """Generate a core file for a process with a single thread in which we know
    that the thread holds the GIL at the time the core was generated and check that we
    can detect that indeed the thread holds the GIL.
    """
    # GIVEN
    _, python_executable = python

    # WHEN

    with generate_core_file(
        python_executable, TEST_SINGLE_THREAD_GIL_FILE, tmpdir
    ) as core_file:
        threads = list(get_process_threads_for_core(core_file, Path(python_executable)))

    # THEN

    assert len(threads) == 1
    (thread,) = threads
    assert thread.holds_the_gil


@ALL_PYTHONS
def test_gil_status_single_thread_does_not_hold_the_gil_for_core(python, tmpdir):
    """Generate a core file for a process with a single thread in which we know
    that the thread does not holds the GIL at the time the core was generated and
    check that we can detect that indeed the thread does not hold the GIL.
    """
    # GIVEN
    _, python_executable = python

    # WHEN

    with generate_core_file(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as core_file:
        threads = list(get_process_threads_for_core(core_file, Path(python_executable)))

    # THEN

    assert len(threads) == 1
    (thread,) = threads
    assert not thread.holds_the_gil
