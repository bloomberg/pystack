import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

from pystack.engine import CoreFileAnalyzer
from pystack.engine import NativeReportingMode
from pystack.engine import StackMethod
from pystack.engine import get_process_threads_for_core
from pystack.errors import EngineError
from pystack.errors import NotEnoughInformation
from pystack.types import LocationInfo
from pystack.types import NativeFrame
from pystack.types import frame_type
from tests.utils import ALL_PYTHONS
from tests.utils import PythonVersion
from tests.utils import all_pystack_combinations
from tests.utils import generate_core_file
from tests.utils import python_has_inlined_eval_frames
from tests.utils import python_has_position_information
from tests.utils import xfail_on_expected_exceptions

CORE_FILE_PATHS = Path(__file__).parent / "corefiles"
TEST_SINGLE_THREAD_FILE = Path(__file__).parent / "single_thread_program.py"
TEST_MULTIPLE_THREADS_FILE = Path(__file__).parent / "multiple_thread_program.py"
VERSION_REGEXP = re.compile(r"python(?P<major>\d+)\.(?P<minor>\d+)")
TEST_PYTHON_AND_OS_THREADS = (
    Path(__file__).parent / "empty_thread_extension_with_os_threads"
)
TEST_INLINE_CALLS_FILE = Path(__file__).parent / "inline_calls_program.py"
TEST_POSITION_INFO_FILE = Path(__file__).parent / "position_information_program.py"
TEST_NO_FRAMES_AT_SHUTDOWN_FILE = (
    Path(__file__).parent / "no_frames_at_shutdown_program.py"
)


@all_pystack_combinations(corefile=True)
def test_single_thread_stack(
    python: PythonVersion, method: StackMethod, blocking: bool, tmpdir: Path
) -> None:
    """Generate a core file for a process with a single thread and check
    that we can inspect and obtain all the information we need from the
    frame stack, including the native frames.
    """
    # GIVEN

    (major_version, minor_version), python_executable = python

    # WHEN

    with generate_core_file(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as core_file:
        with xfail_on_expected_exceptions(method):
            threads = list(
                get_process_threads_for_core(
                    core_file, python_executable, method=method
                )
            )

    # THEN

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
    if python_has_inlined_eval_frames(major_version, minor_version):
        assert len(eval_frames) == 1
    else:
        assert len(eval_frames) >= 4
    assert all("?" not in frame.symbol for frame in eval_frames)
    assert all(frame.linenumber != 0 for frame in eval_frames if "?" not in frame.path)


@ALL_PYTHONS
def test_single_thread_stack_from_elf_data(python: PythonVersion, tmpdir: Path) -> None:
    # GIVEN

    (major_version, minor_version), python_executable = python

    # WHEN

    with generate_core_file(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as core_file:
        if major_version >= 3 and minor_version >= 10:
            threads = list(
                get_process_threads_for_core(
                    core_file, python_executable, method=StackMethod.ELF_DATA
                )
            )
        else:
            with pytest.raises(NotEnoughInformation):
                threads = list(
                    get_process_threads_for_core(
                        core_file, python_executable, method=StackMethod.ELF_DATA
                    )
                )
            return

    # THEN

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
    if python_has_inlined_eval_frames(major_version, minor_version):
        assert len(eval_frames) == 1
    else:
        assert len(eval_frames) >= 4
    assert all("?" not in frame.symbol for frame in eval_frames)
    assert all(frame.linenumber != 0 for frame in eval_frames if "?" not in frame.path)


@all_pystack_combinations(corefile=True)
def test_multiple_thread_stack_native(
    python: PythonVersion, method: StackMethod, blocking: bool, tmpdir: Path
) -> None:
    """Generate a core file for a process with a multiple threads and check
    that we can inspect and obtain all the information we need from the
    frame stack, including the native frames.
    """
    # GIVEN
    (major_version, minor_version), python_executable = python

    # WHEN

    with generate_core_file(
        python_executable, TEST_MULTIPLE_THREADS_FILE, tmpdir
    ) as core_file:
        with xfail_on_expected_exceptions(method):
            threads = list(
                get_process_threads_for_core(
                    core_file, python_executable, method=method
                )
            )

    # THEN

    assert len(threads) == 4
    main_thread = next(
        thread
        for thread in threads
        if thread.frame and "threading" not in thread.frame.code.filename
    )
    other_threads = [thread for thread in threads if thread != main_thread]

    frames = list(main_thread.frames)
    assert (len(frames)) == 4

    filenames = {frame.code.filename for frame in frames}
    assert filenames == {str(TEST_MULTIPLE_THREADS_FILE)}

    functions = [frame.code.scope for frame in frames]
    assert functions == ["<module>", "first_func", "second_func", "third_func"]

    *line_numbers, last_line = [frame.code.location.lineno for frame in frames]
    assert line_numbers == [42, 23, 27]
    assert last_line in {38, 39}

    assert main_thread.native_frames
    eval_frames = [
        frame
        for frame in main_thread.native_frames
        if frame_type(frame, main_thread.python_version) == NativeFrame.FrameType.EVAL
    ]
    if python_has_inlined_eval_frames(major_version, minor_version):
        assert len(eval_frames) == 1
    else:
        assert len(eval_frames) >= 4
    assert all("?" not in frame.symbol for frame in eval_frames)
    assert all(frame.linenumber != 0 for frame in eval_frames if "?" not in frame.path)

    for thread in other_threads:
        frames = [
            frame for frame in thread.frames if "threading" not in frame.code.filename
        ]
        assert (len(frames)) == 3

        filenames = {frame.code.filename for frame in frames}
        assert str(TEST_MULTIPLE_THREADS_FILE) in filenames

        functions = [frame.code.scope for frame in frames]
        assert functions == ["thread_func_1", "thread_func_2", "thread_func_3"]

        *line_numbers, last_line = [frame.code.location.lineno for frame in frames]
        assert line_numbers == [9, 13]
        assert last_line in {18, 19}

        assert thread.native_frames
        eval_frames = [
            frame
            for frame in thread.native_frames
            if frame_type(frame, thread.python_version) == NativeFrame.FrameType.EVAL
        ]
        if python_has_inlined_eval_frames(major_version, minor_version):
            assert len(eval_frames) == 2
        else:
            assert len(eval_frames) >= 6
        assert all("?" not in frame.symbol for frame in eval_frames)
        assert all(
            frame.linenumber != 0 for frame in eval_frames if "?" not in frame.path
        )


def test_thread_registered_with_python_with_other_threads(tmpdir):
    # WHEN
    extension_name = "empty_thread_extension_with_os_threads"
    extension_path = tmpdir / extension_name
    shutil.copytree(TEST_PYTHON_AND_OS_THREADS, extension_path)
    subprocess.run(
        [sys.executable, str(extension_path / "setup.py"), "build_ext", "--inplace"],
        check=True,
        cwd=extension_path,
        capture_output=True,
    )

    extension_executable = extension_path / "main.py"
    with generate_core_file(sys.executable, extension_executable, tmpdir) as core_file:
        threads = list(
            get_process_threads_for_core(
                core_file, Path(sys.executable), native_mode=NativeReportingMode.ALL
            )
        )

    # THEN
    assert len(threads) == 3
    (main_thread, second_thread, non_python_thread) = threads

    main_frames = list(main_thread.frames)
    assert not main_frames
    assert main_thread.native_frames
    assert any("sleepThread" in frame.symbol for frame in main_thread.native_frames)

    frames = list(second_thread.frames)
    assert (len(frames)) == 2

    filenames = {frame.code.filename for frame in frames}
    assert filenames == {str(extension_executable)}

    functions = [frame.code.scope for frame in frames]
    assert functions == ["<module>", "foo"]

    lines = [frame.code.location.lineno for frame in frames]
    assert lines == [13, 10]

    native_frames = list(non_python_thread.native_frames)
    assert len(native_frames) >= 5
    symbols = {frame.symbol for frame in native_frames}
    assert any(
        expected_symbol in symbols
        for expected_symbol in {"sleep", "__nanosleep", "nanosleep"}
    )


@ALL_PYTHONS
def test_inlined_python_calls(python, tmpdir):
    # GIVEN

    (major_version, minor_version), python_executable = python

    # WHEN

    with generate_core_file(
        python_executable, TEST_INLINE_CALLS_FILE, tmpdir
    ) as core_file:
        threads = list(get_process_threads_for_core(core_file, python_executable))

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 6

    filenames = {frame.code.filename for frame in frames}
    assert filenames == {str(TEST_INLINE_CALLS_FILE)}

    functions = [frame.code.scope for frame in frames]
    assert functions == ["<module>", "ham", "spam", "foo", "bar", "baz"]

    if python_has_inlined_eval_frames(major_version, minor_version):
        module, ham, spam, foo, bar, baz = frames
        assert module.is_entry
        assert not ham.is_entry
        assert not spam.is_entry
        assert foo.is_entry
        assert not bar.is_entry
        assert not baz.is_entry
    else:
        assert all(frame.is_entry for frame in frames)


@ALL_PYTHONS
def test_position_information(python, tmpdir):
    # GIVEN

    (major_version, minor_version), python_executable = python

    # WHEN

    with generate_core_file(
        python_executable, TEST_POSITION_INFO_FILE, tmpdir
    ) as core_file:
        threads = list(get_process_threads_for_core(core_file, python_executable))

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 5

    filenames = {frame.code.filename for frame in frames}
    assert filenames == {str(TEST_POSITION_INFO_FILE)}

    functions = [frame.code.scope for frame in frames]
    assert functions == [
        "<module>",
        "first_func",
        "__getitem__",
        "second_func",
        "third_func",
    ]

    positions = [frame.code.location for frame in frames]
    if python_has_position_information(major_version, minor_version):
        assert positions == [
            LocationInfo(lineno=32, end_lineno=32, column=0, end_column=13),
            LocationInfo(lineno=14, end_lineno=14, column=12, end_column=20),
            LocationInfo(lineno=9, end_lineno=9, column=15, end_column=32),
            LocationInfo(lineno=20, end_lineno=22, column=4, end_column=5),
            LocationInfo(lineno=29, end_lineno=29, column=4, end_column=20),
        ]
    else:
        assert positions == [
            LocationInfo(lineno=32, end_lineno=32, column=0, end_column=0),
            LocationInfo(lineno=14, end_lineno=14, column=0, end_column=0),
            LocationInfo(lineno=9, end_lineno=9, column=0, end_column=0),
            (
                LocationInfo(lineno=20, end_lineno=20, column=0, end_column=0)
                if (major_version, minor_version) >= (3, 8)
                else LocationInfo(lineno=21, end_lineno=21, column=0, end_column=0)
            ),
            LocationInfo(lineno=29, end_lineno=29, column=0, end_column=0),
        ]


@ALL_PYTHONS
def test_no_frames_at_shutdown(python, tmpdir):
    # GIVEN

    (major_version, minor_version), python_executable = python

    # WHEN

    with generate_core_file(
        python_executable, TEST_NO_FRAMES_AT_SHUTDOWN_FILE, tmpdir
    ) as core_file:
        threads = list(get_process_threads_for_core(core_file, python_executable))

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    if major_version > 2:
        assert not frames
    else:
        assert len(frames) == 1
        (frame,) = frames
        assert frame.code.scope == "_run_exitfuncs"


def test_get_pid_from_core(tmpdir: Path) -> None:
    # GIVEN
    fifo = tmpdir / "the_fifo"
    os.mkfifo(fifo)
    process = subprocess.Popen(
        [sys.executable, TEST_SINGLE_THREAD_FILE, str(fifo)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    expected_pid = process.pid

    with open(fifo, "r") as fifo_file:
        response = fifo_file.read()

    assert response == "ready"
    subprocess.run(
        ["gcore", str(process.pid)],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=str(tmpdir),
        text=True,
    )
    (core,) = Path(tmpdir).glob("core.*")
    process.terminate()
    process.kill()

    # WHEN

    core_map_analyzer = CoreFileAnalyzer(str(core), sys.executable)
    pid = core_map_analyzer.extract_pid()

    # THEN

    assert pid == expected_pid


@ALL_PYTHONS
def test_extract_executable(python: PythonVersion, tmpdir: Path) -> None:
    """Generate a core file for a process with a single thread and check
    that we can extract the executable that was used to create the core file"""

    # GIVEN
    _, python_executable = python

    # WHEN
    with generate_core_file(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as core_file:
        core_map_analyzer = CoreFileAnalyzer(str(core_file))
        executable = core_map_analyzer.extract_executable()

    # THEN

    assert executable == python_executable


def test_extract_failure_info_with_segfault():
    corefile = CORE_FILE_PATHS / "segfault.core"
    core_map_analyzer = CoreFileAnalyzer(str(corefile))
    assert core_map_analyzer.extract_failure_info() == {
        "si_signo": 11,
        "si_errno": 0,
        "si_code": 1,
        "sender_pid": 0,
        "sender_uid": 0,
        "failed_addr": 123456789,
    }


def test_extract_failure_info_with_singal():
    corefile = CORE_FILE_PATHS / "signal.core"
    core_map_analyzer = CoreFileAnalyzer(str(corefile))
    assert core_map_analyzer.extract_failure_info() == {
        "si_signo": 7,
        "si_errno": 0,
        "si_code": 0,
        "sender_pid": 1,
        "sender_uid": 0,
        "failed_addr": 0,
    }


def test_extract_psinfo():
    corefile = CORE_FILE_PATHS / "segfault.core"
    core_map_analyzer = CoreFileAnalyzer(str(corefile))
    assert core_map_analyzer.extract_ps_info() == {
        "state": 0,
        "sname": 82,
        "zomb": 0,
        "nice": 0,
        "flag": 4212480,
        "uid": 0,
        "gid": 0,
        "pid": 75639,
        "ppid": 1,
        "pgrp": 75639,
        "sid": 1,
        "fname": "a.out",
        "psargs": "./a.out ",
    }


def test_extract_psinfo_with_long_name():
    corefile = CORE_FILE_PATHS / "core_with_big_name.core"
    core_map_analyzer = CoreFileAnalyzer(str(corefile))
    psinfo = core_map_analyzer.extract_ps_info()

    assert len(psinfo["fname"]) == 15
    assert len(psinfo["psargs"]) == 79


def test_core_analizer_raises_when_an_invalid_core_is_provided(tmpdir: Path) -> None:
    # GIVEN

    not_a_core = __file__

    # WHEN / THEN
    with pytest.raises(EngineError):
        list(get_process_threads_for_core(Path(not_a_core), Path(sys.executable)))


def test_get_build_ids_from_core(tmpdir: Path) -> None:
    # GIVEN
    python_executable = sys.executable

    # WHEN
    with generate_core_file(
        Path(python_executable), TEST_SINGLE_THREAD_FILE, tmpdir
    ) as core_file:
        # NOTE: `extract_build_ids` can fail if no executable is provided, but
        # pystack always provides an executable when calling extract_build_ids,
        # found using `extract_executable` if the user didn't provide it.
        executable = CoreFileAnalyzer(str(core_file)).extract_executable()
        core_map_analyzer = CoreFileAnalyzer(str(core_file), executable=executable)
        build_ids = set(core_map_analyzer.extract_build_ids())

    # THEN

    assert build_ids
    assert any(elf_id == core_id is not None for _, core_id, elf_id in build_ids)
    for filename, core_id, elf_id in build_ids:
        assert filename is not None and filename != ""
        assert core_id == elf_id
