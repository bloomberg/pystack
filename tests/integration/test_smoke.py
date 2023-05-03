import sys
from pathlib import Path

import pytest

from pystack.engine import CoreFileAnalyzer
from pystack.engine import NativeReportingMode
from pystack.engine import StackMethod
from pystack.engine import get_process_threads
from pystack.engine import get_process_threads_for_core
from tests.utils import generate_core_file
from tests.utils import spawn_child_process
from tests.utils import xfail_on_expected_exceptions

TEST_SINGLE_THREAD_FILE = Path(__file__).parent / "single_thread_program.py"

if sys.version_info < (3, 10):  # pragma: no cover
    STACK_METHODS = (StackMethod.SYMBOLS, StackMethod.BSS, StackMethod.HEAP)
    CORE_STACK_METHODS = (StackMethod.SYMBOLS, StackMethod.BSS)
elif sys.version_info < (3, 11):  # pragma: no cover
    STACK_METHODS = (StackMethod.SYMBOLS, StackMethod.ELF_DATA, StackMethod.HEAP)
    CORE_STACK_METHODS = (StackMethod.SYMBOLS, StackMethod.ELF_DATA)
else:  # pragma: no cover
    STACK_METHODS = (StackMethod.SYMBOLS, StackMethod.ELF_DATA)
    CORE_STACK_METHODS = (StackMethod.SYMBOLS, StackMethod.ELF_DATA)


@pytest.mark.parametrize("method", STACK_METHODS)
@pytest.mark.parametrize("blocking", [True, False], ids=["blocking", "non-blocking"])
def test_simple_execution(method, blocking, tmpdir):
    """Test that we can retrieve the thread state of a single process.

    This test is specially useful to run under valgrind or similar tools and to
    quickly check that the very basic functionality works as expected.
    """

    # WHEN
    with spawn_child_process(
        sys.executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        with xfail_on_expected_exceptions(method):
            threads = list(
                get_process_threads(
                    child_process.pid, method=method, stop_process=blocking
                )
            )

    # THEN
    assert threads is not None


@pytest.mark.parametrize("method", STACK_METHODS)
def test_simple_execution_native(method, tmpdir):
    """Test that we can retrieve the thread state of a single process.

    This test is specially useful to run under valgrind or similar tools and to
    quickly check that the very basic functionality works as expected.
    """

    # WHEN
    with spawn_child_process(
        sys.executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        with xfail_on_expected_exceptions(method):
            threads = list(
                get_process_threads(
                    child_process.pid,
                    method=method,
                    native_mode=NativeReportingMode.PYTHON,
                )
            )

    # THEN
    assert threads is not None


@pytest.mark.parametrize("method", CORE_STACK_METHODS)
def test_simple_execution_for_core(method, tmpdir):
    """Test that we can retrieve the thread state of a single core file.

    This test is specially useful to run under valgrind or similar tools and to
    quickly check that the very basic functionality works as expected.
    """

    # WHEN

    with generate_core_file(
        Path(sys.executable), TEST_SINGLE_THREAD_FILE, tmpdir
    ) as core_file:
        threads = list(
            get_process_threads_for_core(core_file, Path(sys.executable), method=method)
        )

    # THEN
    assert threads is not None


def test_extract_executable_from_core(tmpdir: Path) -> None:
    """Generate a core file for a process with a single thread and check
    that we can extract the executable that was used to create the core file"""

    # WHEN
    with generate_core_file(
        Path(sys.executable), TEST_SINGLE_THREAD_FILE, tmpdir
    ) as core_file:
        core_map_analyzer = CoreFileAnalyzer(str(core_file))
        executable = core_map_analyzer.extract_executable()

    # THEN

    assert executable == Path(sys.executable)
