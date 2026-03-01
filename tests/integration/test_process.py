import sys
import threading
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import pytest

from pystack._pystack import ProcessManager
from pystack.engine import get_process_threads
from pystack.process import is_elf
from tests.utils import ALL_PYTHONS
from tests.utils import spawn_child_process

TEST_SINGLE_THREAD_FILE = Path(__file__).parent / "single_thread_program.py"
TEST_SHUTDOWN_FILE = Path(__file__).parent / "shutdown_program.py"


@ALL_PYTHONS
def test_detection_of_interpreter_shutdown(python, tmpdir):
    # GIVEN
    _, python_executable = python

    with spawn_child_process(
        python_executable, TEST_SHUTDOWN_FILE, tmpdir
    ) as child_process:
        process_manager = ProcessManager.create_from_pid(
            child_process.pid, stop_process=True
        )

        # WHEN
        is_running = process_manager.is_interpreter_active()
        status = process_manager.interpreter_status()

        # THEN

    assert status == -1 or is_running is False


@ALL_PYTHONS
def test_detection_of_interpreter_active(python, tmpdir):
    # GIVEN
    _, python_executable = python

    with spawn_child_process(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        process_manager = ProcessManager.create_from_pid(
            child_process.pid, stop_process=True
        )

        # WHEN
        is_running = process_manager.is_interpreter_active()
        status = process_manager.interpreter_status()

    # THEN

    assert status == -1 or is_running is True


@ALL_PYTHONS
def test_reattaching_to_already_traced_process(python, tmpdir):
    # GIVEN
    _, python_executable = python

    with spawn_child_process(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        pid = child_process.pid

        # WHEN / THEN
        # Use threading to create overlapping attachment attempts.
        # The first thread holds the ptrace attachment while the second tries to attach.
        barrier = threading.Barrier(2)
        results = []
        errors = []

        def attach_thread():
            try:
                barrier.wait(timeout=5)  # Synchronize start
                threads = list(get_process_threads(pid, stop_process=True))
                results.append(len(threads))
            except Exception as e:
                errors.append(str(e))

        with ThreadPoolExecutor(max_workers=2) as executor:
            f1 = executor.submit(attach_thread)
            f2 = executor.submit(attach_thread)
            f1.result(timeout=10)
            f2.result(timeout=10)

        # One should succeed, one should fail with "Operation not permitted"
        assert len(results) + len(errors) == 2
        assert any("Operation not permitted" in err for err in errors)


@pytest.mark.parametrize(
    "file, expected",
    [
        (sys.executable, True),
        (__file__, False),
        ("/etc", False),
    ],
)
def test_elf_checker(file, expected):
    assert is_elf(file) == expected


def test_invalid_method_for_get_process_threads():
    # GIVEN/WHEN/THEN
    with pytest.raises(ValueError, match="Invalid method for stack analysis"):
        list(
            get_process_threads(
                pid=1,
                method=None,  # type: ignore
            )
        )
