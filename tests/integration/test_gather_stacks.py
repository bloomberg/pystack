import shutil
import subprocess
import sys
from pathlib import Path
from unittest.mock import mock_open
from unittest.mock import patch

import pytest

from pystack.engine import NativeReportingMode
from pystack.engine import StackMethod
from pystack.engine import get_process_threads
from pystack.errors import NotEnoughInformation
from pystack.maps import MAPS_REGEXP
from pystack.process import get_thread_name
from pystack.types import LocationInfo
from pystack.types import NativeFrame
from pystack.types import frame_type
from tests.utils import ALL_PYTHONS
from tests.utils import all_pystack_combinations
from tests.utils import python_has_inlined_eval_frames
from tests.utils import python_has_position_information
from tests.utils import spawn_child_process
from tests.utils import xfail_on_expected_exceptions

TEST_SINGLE_THREAD_FILE = Path(__file__).parent / "single_thread_program.py"
TEST_MULTIPLE_THREADS_FILE = Path(__file__).parent / "multiple_thread_program.py"
TEST_EMPTY_THREAD_EXTENSION = Path(__file__).parent / "empty_thread_extension"
TEST_PYTHON_AND_OS_THREADS = (
    Path(__file__).parent / "empty_thread_extension_with_os_threads"
)
TEST_INLINE_CALLS_FILE = Path(__file__).parent / "inline_calls_program.py"
TEST_POSITION_INFO_FILE = Path(__file__).parent / "position_information_program.py"
TEST_NO_FRAMES_AT_SHUTDOWN_FILE = (
    Path(__file__).parent / "no_frames_at_shutdown_program.py"
)


@all_pystack_combinations()
def test_single_thread_stack(python, blocking, method, tmpdir):
    # GIVEN

    _, python_executable = python

    # WHEN

    with spawn_child_process(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        with xfail_on_expected_exceptions(method):
            threads = list(
                get_process_threads(
                    child_process.pid, stop_process=blocking, method=method
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

    assert not thread.native_frames


def test_single_thread_stack_non_blocking(tmpdir):
    # GIVEN / WHEN

    with spawn_child_process(
        sys.executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        threads = list(get_process_threads(child_process.pid, stop_process=False))

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

    assert not thread.native_frames


@all_pystack_combinations()
def test_multiple_thread_stack(python, blocking, method, tmpdir):
    # GIVEN

    _, python_executable = python

    # WHEN

    with spawn_child_process(
        python_executable, TEST_MULTIPLE_THREADS_FILE, tmpdir
    ) as child_process:
        with xfail_on_expected_exceptions(method):
            threads = list(
                get_process_threads(
                    child_process.pid, stop_process=blocking, method=method
                )
            )

    # THEN

    assert len(threads) == 4
    main_thread = next(
        thread for thread in threads if "threading" not in thread.frame.code.filename
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

    assert not main_thread.native_frames

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

        assert not thread.native_frames


@all_pystack_combinations(native=True)
def test_single_thread_stack_native(python, method, blocking, tmpdir):
    # GIVEN

    (major_version, minor_version), python_executable = python

    # WHEN

    with spawn_child_process(
        python_executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        with xfail_on_expected_exceptions(method):
            threads = list(
                get_process_threads(
                    child_process.pid,
                    native_mode=NativeReportingMode.PYTHON,
                    stop_process=blocking,
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
    if python_has_inlined_eval_frames(major_version, minor_version):  # pragma: no cover
        assert len(eval_frames) == 1
    else:  # pragma: no cover
        assert len(eval_frames) >= 4
    assert all("?" not in frame.symbol for frame in eval_frames)
    if any(frame.linenumber == 0 for frame in eval_frames):  # pragma: no cover
        assert all(frame.linenumber == 0 for frame in eval_frames)
        assert all(frame.path == "???" in frame.path for frame in eval_frames)
    else:  # pragma: no cover
        assert all(frame.linenumber != 0 for frame in eval_frames)
        assert any(frame.path and "?" not in frame.path for frame in eval_frames)


@all_pystack_combinations(native=True)
def test_multiple_thread_stack_native(python, method, blocking, tmpdir):
    # GIVEN

    (major_version, minor_version), python_executable = python

    # WHEN

    with spawn_child_process(
        python_executable, TEST_MULTIPLE_THREADS_FILE, tmpdir
    ) as child_process:
        with xfail_on_expected_exceptions(method):
            threads = list(
                get_process_threads(
                    child_process.pid,
                    native_mode=NativeReportingMode.PYTHON,
                    stop_process=blocking,
                )
            )

    # THEN

    assert len(threads) == 4
    main_thread = next(
        thread for thread in threads if "threading" not in thread.frame.code.filename
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
    if python_has_inlined_eval_frames(major_version, minor_version):  # pragma: no cover
        assert len(eval_frames) == 1
    else:  # pragma: no cover
        assert len(eval_frames) >= 4
    assert all("?" not in frame.symbol for frame in eval_frames)
    if any(frame.linenumber == 0 for frame in eval_frames):  # pragma: no cover
        assert all(frame.linenumber == 0 for frame in eval_frames)
        assert all(frame.path == "???" in frame.path for frame in eval_frames)
    else:  # pragma: no cover
        assert all(frame.linenumber != 0 for frame in eval_frames)
        assert any(frame.path and "?" not in frame.path for frame in eval_frames)

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
        if python_has_inlined_eval_frames(
            int(major_version), int(minor_version)
        ):  # pragma: no cover
            assert len(eval_frames) == 2
        else:  # pragma: no cover
            assert len(eval_frames) == 6
        assert all("?" not in frame.symbol for frame in eval_frames)
        if any(frame.linenumber == 0 for frame in eval_frames):  # pragma: no cover
            assert all(frame.linenumber == 0 for frame in eval_frames)
            assert all(frame.path == "???" in frame.path for frame in eval_frames)
        else:  # pragma: no cover
            assert all(frame.linenumber != 0 for frame in eval_frames)
            assert any(frame.path and "?" not in frame.path for frame in eval_frames)


def test_gather_stack_with_heap_fails_if_no_heap(tmpdir):
    # GIVEN / WHEN

    with spawn_child_process(
        sys.executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        the_data = []
        with open(f"/proc/{child_process.pid}/maps") as f:
            for line in f.readlines():
                match = MAPS_REGEXP.match(line)
                assert match is not None
                if match.group("pathname") and "[heap]" in match.group("pathname"):
                    line = line.replace("[heap]", "[mysterious_segment]")
                the_data.append(line)
        data = "".join(the_data)
        with patch("builtins.open", mock_open(read_data=data)):
            # THEN

            with pytest.raises(NotEnoughInformation):
                list(
                    get_process_threads(
                        child_process.pid, stop_process=True, method=StackMethod.HEAP
                    )
                )


def test_gather_stack_with_bss_fails_if_no_bss(tmpdir):
    # GIVEN / WHEN

    with spawn_child_process(
        sys.executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        the_data = []
        with open(f"/proc/{child_process.pid}/maps") as f:
            for line in f.readlines():
                match = MAPS_REGEXP.match(line)
                assert match is not None
                if not match.group("pathname"):
                    line = line.replace("\n", "[mysterious_segment]\n")
                the_data.append(line)

        data = "".join(the_data)

        with patch("builtins.open", mock_open(read_data=data)), patch(
            "pystack.maps._get_bss", return_value=None
        ):
            # THEN

            with pytest.raises(NotEnoughInformation):
                list(
                    get_process_threads(
                        child_process.pid, stop_process=True, method=StackMethod.BSS
                    )
                )


def test_gather_stack_auto_works_if_no_bss(tmpdir):
    # GIVEN / WHEN

    with spawn_child_process(
        sys.executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        the_data = []
        with open(f"/proc/{child_process.pid}/maps") as f:
            for line in f.readlines():
                match = MAPS_REGEXP.match(line)
                assert match is not None
                if not match.group("pathname"):
                    line = line.replace("\n", "[mysterious_segment]\n")
                the_data.append(line)
        data = "".join(the_data)
        with patch("builtins.open", mock_open(read_data=data)), patch(
            "pystack.maps._get_bss", return_value=None
        ):
            threads = list(
                get_process_threads(
                    child_process.pid, stop_process=True, method=StackMethod.AUTO
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

    assert not thread.native_frames


def test_gather_stack_auto_works_if_no_heap(tmpdir):
    # GIVEN / WHEN

    with spawn_child_process(
        sys.executable, TEST_SINGLE_THREAD_FILE, tmpdir
    ) as child_process:
        the_data = []
        with open(f"/proc/{child_process.pid}/maps") as f:
            for line in f.readlines():
                match = MAPS_REGEXP.match(line)
                assert match is not None
                if match.group("pathname") and "[heap]" in match.group("pathname"):
                    line = line.replace("[heap]", "[mysterious_segment]")
                the_data.append(line)
        data = "".join(the_data)
        with patch("builtins.open", mock_open(read_data=data)):
            threads = list(
                get_process_threads(
                    child_process.pid, stop_process=True, method=StackMethod.AUTO
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

    assert not thread.native_frames


@ALL_PYTHONS
def test_thread_registered_with_python_but_with_no_python_calls(python, tmpdir):
    # GIVEN

    _, python_executable = python

    # WHEN
    extension_name = "empty_thread_extension"
    extension_path = tmpdir / extension_name
    shutil.copytree(TEST_EMPTY_THREAD_EXTENSION, extension_path)
    subprocess.run(
        [python_executable, str(extension_path / "setup.py"), "build_ext", "--inplace"],
        check=True,
        cwd=extension_path,
        capture_output=True,
    )

    extension_executable = extension_path / "main.py"
    with spawn_child_process(
        python_executable, extension_executable, tmpdir
    ) as child_process:
        threads = list(
            get_process_threads(
                child_process.pid, native_mode=NativeReportingMode.PYTHON
            )
        )

    # THEN

    assert len(threads) == 2
    (main_thread, second_thread) = threads

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
    with spawn_child_process(
        sys.executable, extension_executable, tmpdir
    ) as child_process:
        threads = list(
            get_process_threads(child_process.pid, native_mode=NativeReportingMode.ALL)
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


def test_get_thread_name(tmpdir):
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
    with spawn_child_process(
        sys.executable, extension_executable, tmpdir
    ) as child_process:
        threads = list(
            get_process_threads(child_process.pid, native_mode=NativeReportingMode.ALL)
        )

    # THEN

    assert "thread_foo" in {thread.name for thread in threads}


def test_get_thread_name_oserror():
    # WHEN
    thread_name = get_thread_name(pid=0, tid=0)

    # THEN
    assert thread_name is None


@ALL_PYTHONS
def test_inlined_python_calls(python, tmpdir):
    # GIVEN

    (major_version, minor_version), python_executable = python

    # WHEN

    with spawn_child_process(
        python_executable, TEST_INLINE_CALLS_FILE, tmpdir
    ) as child_process:
        threads = list(get_process_threads(child_process.pid))

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 6

    filenames = {frame.code.filename for frame in frames}
    assert filenames == {str(TEST_INLINE_CALLS_FILE)}

    functions = [frame.code.scope for frame in frames]
    assert functions == ["<module>", "ham", "spam", "foo", "bar", "baz"]

    if python_has_inlined_eval_frames(major_version, minor_version):  # pragma: no cover
        module, ham, spam, foo, bar, baz = frames
        assert module.is_entry
        assert not ham.is_entry
        assert not spam.is_entry
        assert foo.is_entry
        assert not bar.is_entry
        assert not baz.is_entry
    else:  # pragma: no cover
        assert all(frame.is_entry for frame in frames)


@ALL_PYTHONS
def test_position_information(python, tmpdir):
    # GIVEN

    (major_version, minor_version), python_executable = python

    # WHEN

    with spawn_child_process(
        python_executable, TEST_POSITION_INFO_FILE, tmpdir
    ) as child_process:
        threads = list(get_process_threads(child_process.pid))

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
    if python_has_position_information(
        major_version, minor_version
    ):  # pragma: no cover
        assert positions == [
            LocationInfo(lineno=32, end_lineno=32, column=0, end_column=13),
            LocationInfo(lineno=14, end_lineno=14, column=12, end_column=20),
            LocationInfo(lineno=9, end_lineno=9, column=15, end_column=32),
            LocationInfo(lineno=20, end_lineno=22, column=4, end_column=5),
            LocationInfo(lineno=29, end_lineno=29, column=4, end_column=20),
        ]
    else:  # pragma: no cover
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

    with spawn_child_process(
        python_executable, TEST_NO_FRAMES_AT_SHUTDOWN_FILE, tmpdir
    ) as child_process:
        threads = list(get_process_threads(child_process.pid))

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    if major_version > 2:  # pragma: no cover
        assert not frames
    else:  # pragma: no cover
        assert len(frames) == 1
        (frame,) = frames
        assert frame.code.scope == "_run_exitfuncs"
