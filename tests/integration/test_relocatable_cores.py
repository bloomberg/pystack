import logging
import subprocess
import sys
from pathlib import Path

import pytest
from pytest import LogCaptureFixture

from pystack.engine import CoreFileAnalyzer
from pystack.engine import StackMethod
from pystack.engine import get_process_threads_for_core
from pystack.types import NativeFrame
from pystack.types import frame_type
from tests.utils import generate_core_file

CORE_FILE_PATHS = Path(__file__).parent / "corefiles"
TEST_SINGLE_THREAD_FILE = Path(__file__).parent / "single_thread_program.py"
TEST_MULTIPLE_THREADS_FILE = Path(__file__).parent / "multiple_thread_program.py"

try:
    import pypungi

    pypungi_missing = False
except ImportError:
    pypungi_missing = True

pytestmark = pytest.mark.skipif(
    pypungi_missing,
    reason="pypungi is not installed, skipping tests that require it",
)


def test_single_thread_stack_for_relocated_core(
    tmpdir: Path, caplog: LogCaptureFixture
) -> None:
    """Generate a core file for a process with a single thread, relocate
    the files that were used to generate the core file and check
    that we can inspect and obtain all the information we need from the
    frame stack, including the native frames by using symbols only.
    """

    # GIVEN

    target_bundle = Path(tmpdir / "bundle")
    subprocess.check_call([sys.executable, "-m", "pypungi", "-o", str(target_bundle)])
    python_executable = Path(target_bundle / "bin" / "python")
    relocated_bundle = Path(tmpdir / "relocated_bundle")
    python_binary = Path(relocated_bundle / "python" / "bin" / "python")
    caplog.set_level(logging.WARNING)

    # WHEN
    with generate_core_file(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as core_file:
        target_bundle.rename(relocated_bundle)
        binary_dependencies = ":".join(
            {str(file.parent) for file in relocated_bundle.glob("**/*.so")}
        )
        threads = list(
            get_process_threads_for_core(
                core_file,
                python_binary,
                library_search_path=binary_dependencies,
                method=StackMethod.SYMBOLS,
            )
        )

    # THEN

    # Check that we didn't generate any internal errors or warnings

    assert not caplog.records

    # Check that we have a bunch of shared libs that we cannot locate without the search path

    core_map_analyzer_no_search_path = CoreFileAnalyzer(str(core_file), python_binary)
    assert core_map_analyzer_no_search_path.missing_modules()

    # Check that all shared libs are located with the search path

    core_map_analyzer_search_path = CoreFileAnalyzer(
        str(core_file), python_binary, binary_dependencies
    )
    assert not core_map_analyzer_search_path.missing_modules()

    # Check that we can normally analyze the core file

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 4

    filenames = {frame.code.filename for frame in frames}
    assert filenames == {str(TEST_SINGLE_THREAD_FILE)}

    functions = [frame.code.scope for frame in frames]
    assert functions == ["<module>", "first_func", "second_func", "third_func"]

    *line_numbers, last_line = [frame.code.location.lineno for frame in frames]
    assert line_numbers == [20, 6, 10]
    assert last_line in {16, 17}

    assert thread.native_frames
    eval_frames = [
        frame
        for frame in thread.native_frames
        if frame_type(frame, thread.python_version) == NativeFrame.FrameType.EVAL
    ]
    assert len(eval_frames) >= 4
    assert all("?" not in frame.symbol for frame in eval_frames)
    assert all(frame.linenumber != 0 for frame in eval_frames if "?" not in frame.path)


def test_missing_shared_libraries(tmpdir: Path) -> None:
    """Generate a core file for a process with a single thread, relocate
    the files that were used to generate the core file and check
    that we can correctly list the missing shared libraries.
    """

    # WHEN
    target_bundle = Path(tmpdir / "bundle")
    subprocess.check_call([sys.executable, "-m", "pypungi", "-o", str(target_bundle)])
    python_executable = Path(target_bundle / "bin" / "python")
    relocated_bundle = Path(tmpdir / "relocated_bundle")
    python_binary = Path(relocated_bundle / "python" / "bin" / "python")

    with generate_core_file(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as core_file:
        target_bundle.rename(relocated_bundle)

    # THEN

    core_map_analyzer_no_search_path = CoreFileAnalyzer(str(core_file), python_binary)
    assert core_map_analyzer_no_search_path.missing_modules()


def test_missing_shared_libraries_with_search_path(tmpdir: Path) -> None:
    """Generate a core file for a process with a single thread, relocate
    the files that were used to generate the core file and check
    that we can find all shared libraries when providing the search path.
    """

    # WHEN
    target_bundle = Path(tmpdir / "bundle")
    subprocess.check_call([sys.executable, "-m", "pypungi", "-o", str(target_bundle)])
    python_executable = Path(target_bundle / "bin" / "python")
    relocated_bundle = Path(tmpdir / "relocated_bundle")
    python_binary = Path(relocated_bundle / "python" / "bin" / "python")

    with generate_core_file(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as core_file:
        target_bundle.rename(relocated_bundle)

    # THEN

    binary_dependencies = ":".join(
        {str(file.parent) for file in relocated_bundle.glob("**/*.so")}
    )
    core_map_analyzer_search_path = CoreFileAnalyzer(
        str(core_file), python_binary, binary_dependencies
    )
    assert not core_map_analyzer_search_path.missing_modules()


def test_invalid_library_path_with_regular_files(tmpdir: Path) -> None:
    """Generate a core file for a process with a single thread and check
    that we can obtain the missing modules even if the library path
    contains elements that are not folders.
    """

    # WHEN
    python = Path(sys.executable)

    with generate_core_file(python, TEST_SINGLE_THREAD_FILE, tmpdir) as core_file:
        pass

    # THEN
    core_map_analyzer = CoreFileAnalyzer(str(core_file), python, python)
    assert not core_map_analyzer.missing_modules()


def test_invalid_library_path_with_invalid_entries(tmpdir: Path) -> None:
    """Generate a core file for a process with a single thread and check
    that we can obtain the missing modules even if the library path
    contains incorrect elements.
    """

    # WHEN
    python = Path(sys.executable)

    with generate_core_file(python, TEST_SINGLE_THREAD_FILE, tmpdir) as core_file:
        pass

    # THEN
    core_map_analyzer = CoreFileAnalyzer(str(core_file), python, "blech blich bluch")
    assert not core_map_analyzer.missing_modules()
