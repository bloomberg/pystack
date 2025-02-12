import sys
from pathlib import Path

import pytest

from pystack._pystack import ProcessManager
from pystack.engine import CoreFileAnalyzer
from pystack.engine import get_process_threads
from pystack.errors import EngineError
from pystack.maps import generate_maps_for_process
from pystack.maps import parse_maps_file
from pystack.maps import parse_maps_file_for_binary
from pystack.process import is_elf
from pystack.process import scan_core_bss_for_python_version
from pystack.process import scan_process_bss_for_python_version
from tests.utils import ALL_PYTHONS
from tests.utils import generate_core_file
from tests.utils import spawn_child_process

TEST_SINGLE_THREAD_FILE = Path(__file__).parent / "single_thread_program.py"
TEST_SHUTDOWN_FILE = Path(__file__).parent / "shutdown_program.py"


@ALL_PYTHONS
def test_remote_version_detection_using_bss_section(python, tmpdir):
    # GIVEN

    (expected_major, expected_minor), python_executable = python

    # WHEN

    with spawn_child_process(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        all_maps = generate_maps_for_process(child_process.pid)
        maps = parse_maps_file(child_process.pid, all_maps)
        major, minor = scan_process_bss_for_python_version(child_process.pid, maps.bss)

    # THEN

    assert major == expected_major
    assert minor == expected_minor


@ALL_PYTHONS
def test_core_version_detection_using_bss_section(python, tmpdir):
    # GIVEN

    (expected_major, expected_minor), python_executable = python

    # WHEN

    with generate_core_file(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as corefile:
        core_map_analyzer = CoreFileAnalyzer(str(corefile), str(python_executable))
        virtual_maps = tuple(core_map_analyzer.extract_maps())
        load_point_by_module = core_map_analyzer.extract_module_load_points()
        maps = parse_maps_file_for_binary(
            python_executable, virtual_maps, load_point_by_module
        )
        major, minor = scan_core_bss_for_python_version(corefile, maps.bss)

    # THEN

    assert major == expected_major
    assert minor == expected_minor


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
        with pytest.raises(EngineError, match="Operation not permitted"):
            it1 = iter(get_process_threads(pid, stop_process=True))
            it2 = iter(get_process_threads(pid, stop_process=True))
            next(it1)
            next(it2)


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
