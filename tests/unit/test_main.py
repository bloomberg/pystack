import logging
from pathlib import Path
from textwrap import dedent
from unittest.mock import Mock
from unittest.mock import call
from unittest.mock import mock_open
from unittest.mock import patch

import pytest

from pystack.__main__ import NO_SUCH_PROCESS_ERROR_MSG
from pystack.__main__ import PERMISSION_ERROR_MSG
from pystack.__main__ import PERMISSION_HELP_TEXT
from pystack.__main__ import format_failureinfo_information
from pystack.__main__ import format_psinfo_information
from pystack.__main__ import main
from pystack.__main__ import produce_error_message
from pystack.engine import NativeReportingMode
from pystack.engine import StackMethod
from pystack.errors import EXECUTABLE_NOT_FOUND_HELP_TEXT
from pystack.errors import INVALID_EXECUTABLE_HELP_TEXT
from pystack.errors import MISSING_EXECUTABLE_MAPS_HELP_TEXT
from pystack.errors import NOT_ENOUGH_INFORMATION_HELP_TEXT
from pystack.errors import CoreExecutableNotFound
from pystack.errors import EngineError
from pystack.errors import InvalidExecutable
from pystack.errors import InvalidPythonProcess
from pystack.errors import MissingExecutableMaps
from pystack.errors import NotEnoughInformation
from pystack.maps import VirtualMap


def test_error_message_wih_permission_error_text():
    """The C++ extension does not have the possibility to
    produce arbitrary exceptions and it mainly emits RuntimeError
    but in some cases we need to print some extra contextual
    information to the user like if there was a permission error.

    Sadly, the only way to map this is based on a pre-made contract
    based on the exception text. This test checks that said contract
    is fulfilled.
    """

    # GIVEN

    exception = RuntimeError(PERMISSION_ERROR_MSG)

    # WHEN

    message = produce_error_message(exception)

    # THEN

    assert PERMISSION_HELP_TEXT in message


def test_error_message_wih_permission_error_test_from_hardening():
    """When pystack tries to attach to a given process when hardening
    level 1 is enabled, the error that we will receive is not a PermissionError
    but a generic EngineError that says "no such process".

    We want to give good instructions on how to proceed to the user so
    we need to ensure we intercept the error raised in this situation and
    print an appropiate error message.
    """

    # GIVEN

    exception = EngineError(NO_SUCH_PROCESS_ERROR_MSG, pid=0)

    # WHEN

    with patch("pathlib.Path.exists", return_value=True):
        message = produce_error_message(exception)

    # THEN

    assert PERMISSION_HELP_TEXT in message


def test_error_message_wih_executable_not_found_text():
    """When pystack fails to automatically determine the
    executable, the error can be a bit intimidating as normally
    reveals some details about what failed when inspecting
    the core file. To make the experience better to the user we
    print a specialized error message that informs the user that
    they can try to provide the executable if this is known.
    """

    # GIVEN

    exception = CoreExecutableNotFound("Some scary error")

    # WHEN

    message = produce_error_message(exception)

    # THEN

    assert EXECUTABLE_NOT_FOUND_HELP_TEXT in message


def test_error_message_wih_invalid_executable():
    """When pystack detects that the executable is not an ELF
    file it can be quite confusing to the user to know what
    the problem is and what needs to be done. To make the experience
    better to the user we print a specialized error message that informs
    the user that they must provide a valid executable.
    """

    # GIVEN

    exception = InvalidExecutable("Some scary error")

    # WHEN

    message = produce_error_message(exception)

    # THEN

    assert INVALID_EXECUTABLE_HELP_TEXT in message


def test_error_message_wih_missing_memory_maps_text():
    """When pystack cannot match the memory maps to the executable,
    the error displayed is quite technical and users cannot easily
    know what they can do to fix the problem. To improve the situation
    we print a specialized error message that informs the user of
    the typical scenarios where this error happens and what can be done
    to fix it.
    """

    # GIVEN

    exception = MissingExecutableMaps("Some scary error")

    # WHEN

    message = produce_error_message(exception)

    # THEN

    assert MISSING_EXECUTABLE_MAPS_HELP_TEXT in message


def test_error_message_wih_permission_error_text_python_exception():
    """Make sure that the PERMISSION_HELP_TEXT is included if
    we catch a Python PermissionError exception.
    """

    # GIVEN

    exception = PermissionError()

    # WHEN

    message = produce_error_message(exception)

    # THEN

    assert PERMISSION_HELP_TEXT in message


def test_error_message_wih_not_enough_information_error_text():
    """Make sure that the NOT_ENOUGH_INFORMATION_HELP_TEXT is included
    if we catch a Python NotEnoughInformation exception.
    """

    # GIVEN

    exception = NotEnoughInformation("Need more info")

    # WHEN

    message = produce_error_message(exception)

    # THEN

    assert NOT_ENOUGH_INFORMATION_HELP_TEXT in message


def test_process_remote_default():
    # GIVEN

    argv = ["pystack", "remote", "31"]

    threads = [Mock(), Mock(), Mock()]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ):
        get_process_threads_mock.return_value = threads
        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        31,
        stop_process=True,
        native_mode=NativeReportingMode.OFF,
        locals=False,
        method=StackMethod.AUTO,
    )
    TracebackPrinterMock.assert_called_once_with(native_mode=NativeReportingMode.OFF)
    assert TracebackPrinterMock.return_value.print_thread.mock_calls == [
        call(thread) for thread in threads
    ]


def test_process_remote_no_block():
    # GIVEN

    argv = ["pystack", "remote", "31", "--no-block"]

    threads = [Mock(), Mock(), Mock()]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ):
        get_process_threads_mock.return_value = threads
        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        31,
        stop_process=False,
        native_mode=NativeReportingMode.OFF,
        locals=False,
        method=StackMethod.AUTO,
    )
    TracebackPrinterMock.assert_called_once_with(native_mode=NativeReportingMode.OFF)
    assert TracebackPrinterMock.return_value.print_thread.mock_calls == [
        call(thread) for thread in threads
    ]


@pytest.mark.parametrize(
    "argument, mode",
    [
        ["--native", NativeReportingMode.PYTHON],
        ["--native-all", NativeReportingMode.ALL],
        ["--native-last", NativeReportingMode.LAST],
    ],
)
def test_process_remote_native(argument, mode):
    # GIVEN

    argv = ["pystack", "remote", "31", argument]

    threads = [Mock(), Mock(), Mock()]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ):
        get_process_threads_mock.return_value = threads
        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        31,
        stop_process=True,
        native_mode=mode,
        locals=False,
        method=StackMethod.AUTO,
    )
    TracebackPrinterMock.assert_called_once_with(native_mode=mode)
    assert TracebackPrinterMock.return_value.print_thread.mock_calls == [
        call(thread) for thread in threads
    ]


def test_process_remote_locals():
    # GIVEN

    argv = ["pystack", "remote", "31", "--locals"]

    threads = [Mock(), Mock(), Mock()]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ):
        get_process_threads_mock.return_value = threads
        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        31,
        stop_process=True,
        native_mode=NativeReportingMode.OFF,
        locals=True,
        method=StackMethod.AUTO,
    )
    TracebackPrinterMock.assert_called_once_with(native_mode=NativeReportingMode.OFF)
    assert TracebackPrinterMock.return_value.print_thread.mock_calls == [
        call(thread) for thread in threads
    ]


def test_process_remote_native_no_block(capsys):
    # GIVEN

    argv = ["pystack", "remote", "31", "--native", "--no-block"]

    threads = [Mock(), Mock(), Mock()]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ):
        get_process_threads_mock.return_value = threads
        # THEN

        with pytest.raises(SystemExit):
            main()

    get_process_threads_mock.assert_not_called()
    TracebackPrinterMock.assert_not_called()
    TracebackPrinterMock.return_value.print_thread.assert_not_called()


def test_process_remote_exhaustive():
    # GIVEN

    argv = ["pystack", "remote", "31", "--exhaustive"]

    threads = [Mock(), Mock(), Mock()]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ):
        get_process_threads_mock.return_value = threads
        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        31,
        stop_process=True,
        native_mode=NativeReportingMode.OFF,
        locals=False,
        method=StackMethod.ALL,
    )
    TracebackPrinterMock.assert_called_once_with(native_mode=NativeReportingMode.OFF)
    assert TracebackPrinterMock.return_value.print_thread.mock_calls == [
        call(thread) for thread in threads
    ]


@pytest.mark.parametrize(
    "exception, exval", [(EngineError, 1), (InvalidPythonProcess, 2)]
)
def test_process_remote_error(exception, exval, capsys):
    # GIVEN

    argv = ["pystack", "remote", "32"]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ):
        get_process_threads_mock.side_effect = exception("Oh no!")
        with pytest.raises(SystemExit) as excinfo:
            main()
        assert excinfo.value.code == exval

    # THEN

    get_process_threads_mock.assert_called_once()
    TracebackPrinterMock.assert_called_once_with(native_mode=NativeReportingMode.OFF)
    TracebackPrinterMock.return_value.print_thread.assert_not_called()
    capture = capsys.readouterr()
    assert "Oh no!" in capture.err


def test_process_core_default_without_executable():
    # GIVEN

    argv = ["pystack", "core", "corefile"]

    threads = [Mock(), Mock(), Mock()]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads_for_core"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.is_elf", return_value=True
    ), patch(
        "pystack.__main__.is_gzip", return_value=False
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ) as core_file_analizer_mock:
        core_file_analizer_mock().extract_executable.return_value = (
            "extracted_executable"
        )
        get_process_threads_mock.return_value = threads
        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        Path("corefile"),
        Path("extracted_executable"),
        library_search_path="",
        native_mode=NativeReportingMode.OFF,
        locals=False,
        method=StackMethod.AUTO,
    )
    TracebackPrinterMock.assert_called_once_with(NativeReportingMode.OFF)
    assert TracebackPrinterMock.return_value.print_thread.mock_calls == [
        call(thread) for thread in threads
    ]


def test_process_core_default_gzip_without_executable():
    # GIVEN

    argv = ["pystack", "core", "corefile.gz"]

    threads = [Mock(), Mock(), Mock()]

    temp_mock_file = Mock()
    temp_mock_file.name = Path("/tmp/file")
    temp_file_context_mock = Mock()
    temp_file_context_mock.__enter__ = Mock(return_value=temp_mock_file)
    temp_file_context_mock.__exit__ = Mock(return_value=None)

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads_for_core"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.is_elf", return_value=True
    ), patch(
        "builtins.open", mock_open(read_data=b"\x1f\x8b")
    ), patch(
        "gzip.open", mock_open(read_data=b"")
    ) as gzip_open_mock, patch(
        "tempfile.NamedTemporaryFile", return_value=temp_file_context_mock
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ) as core_file_analizer_mock:
        core_file_analizer_mock().extract_executable.return_value = (
            "extracted_executable"
        )
        get_process_threads_mock.return_value = threads

        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        Path("/tmp/file"),
        Path("extracted_executable"),
        library_search_path="",
        native_mode=NativeReportingMode.OFF,
        locals=False,
        method=StackMethod.AUTO,
    )
    TracebackPrinterMock.assert_called_once_with(NativeReportingMode.OFF)
    assert TracebackPrinterMock.return_value.print_thread.mock_calls == [
        call(thread) for thread in threads
    ]
    gzip_open_mock.assert_called_with(Path("corefile.gz"), "rb")


def test_process_core_default_without_executable_and_executable_does_not_exist(capsys):
    # GIVEN

    argv = ["pystack", "core", "corefile"]

    # WHEN

    with patch("sys.argv", argv), patch(
        "pathlib.Path.exists"
    ) as path_exists_mock, patch("pystack.__main__.is_gzip", return_value=False), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ) as core_file_analizer_mock:
        core_file_analizer_mock().extract_executable.return_value = (
            "extracted_executable"
        )
        # THEN
        path_exists_mock.side_effect = [True, False, False, False]

        with pytest.raises(SystemExit):
            main()

    capture = capsys.readouterr()
    assert "You can try to provide one" in capture.err


def test_process_core_executable_not_elf_file(capsys):
    # GIVEN

    argv = ["pystack", "core", "corefile", "executable"]

    threads = [Mock(), Mock(), Mock()]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads_for_core"
    ) as get_process_threads_mock, patch("sys.argv", argv), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ), patch(
        "pystack.__main__.is_elf", return_value=False
    ), patch(
        "pystack.__main__.is_gzip", return_value=False
    ):
        get_process_threads_mock.return_value = threads
        with pytest.raises(SystemExit):
            main()

    capture = capsys.readouterr()
    assert INVALID_EXECUTABLE_HELP_TEXT in capture.err


def test_process_core_default_with_executable():
    # GIVEN

    argv = ["pystack", "core", "corefile", "executable"]

    threads = [Mock(), Mock(), Mock()]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads_for_core"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ), patch(
        "pystack.__main__.is_elf", return_value=True
    ), patch(
        "pystack.__main__.is_gzip", return_value=False
    ):
        get_process_threads_mock.return_value = threads
        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        Path("corefile"),
        Path("executable"),
        library_search_path="",
        native_mode=NativeReportingMode.OFF,
        locals=False,
        method=StackMethod.AUTO,
    )
    TracebackPrinterMock.assert_called_once_with(NativeReportingMode.OFF)
    assert TracebackPrinterMock.return_value.print_thread.mock_calls == [
        call(thread) for thread in threads
    ]


@pytest.mark.parametrize(
    "argument, mode",
    [
        ["--native", NativeReportingMode.PYTHON],
        ["--native-all", NativeReportingMode.ALL],
        ["--native-last", NativeReportingMode.LAST],
    ],
)
def test_process_core_native(argument, mode):
    # GIVEN

    argv = ["pystack", "core", "corefile", "executable", argument]

    threads = [Mock(), Mock(), Mock()]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads_for_core"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ), patch(
        "pystack.__main__.is_elf", return_value=True
    ), patch(
        "pystack.__main__.is_gzip", return_value=False
    ):
        get_process_threads_mock.return_value = threads
        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        Path("corefile"),
        Path("executable"),
        library_search_path="",
        native_mode=mode,
        locals=False,
        method=StackMethod.AUTO,
    )
    TracebackPrinterMock.assert_called_once_with(mode)
    assert TracebackPrinterMock.return_value.print_thread.mock_calls == [
        call(thread) for thread in threads
    ]


def test_process_core_locals():
    # GIVEN

    argv = ["pystack", "core", "corefile", "executable", "--locals"]

    threads = [Mock(), Mock(), Mock()]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads_for_core"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ), patch(
        "pystack.__main__.is_elf", return_value=True
    ), patch(
        "pystack.__main__.is_gzip", return_value=False
    ):
        get_process_threads_mock.return_value = threads
        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        Path("corefile"),
        Path("executable"),
        library_search_path="",
        native_mode=NativeReportingMode.OFF,
        locals=True,
        method=StackMethod.AUTO,
    )
    TracebackPrinterMock.assert_called_once_with(NativeReportingMode.OFF)
    assert TracebackPrinterMock.return_value.print_thread.mock_calls == [
        call(thread) for thread in threads
    ]


def test_process_core_with_search_path():
    # GIVEN

    argv = [
        "pystack",
        "core",
        "corefile",
        "executable",
        "--lib-search-path",
        "foo:bar:baz",
    ]

    threads = [Mock(), Mock(), Mock()]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads_for_core"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ), patch(
        "pystack.__main__.is_elf", return_value=True
    ), patch(
        "pystack.__main__.is_gzip", return_value=False
    ):
        get_process_threads_mock.return_value = threads
        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        Path("corefile"),
        Path("executable"),
        library_search_path="foo:bar:baz",
        native_mode=NativeReportingMode.OFF,
        locals=False,
        method=StackMethod.AUTO,
    )
    TracebackPrinterMock.assert_called_once_with(NativeReportingMode.OFF)
    assert TracebackPrinterMock.return_value.print_thread.mock_calls == [
        call(thread) for thread in threads
    ]


def test_process_core_with_search_root():
    # GIVEN

    argv = ["pystack", "core", "corefile", "executable", "--lib-search-root", "foo"]

    threads = [Mock(), Mock(), Mock()]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads_for_core"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ), patch(
        "pathlib.Path.glob", return_value=[Path("foo/lel.so"), Path("bar/lel.so")]
    ), patch(
        "os.path.isdir",
        return_value=True,
    ), patch(
        "os.access",
        return_value=True,
    ), patch(
        "pystack.__main__.is_elf", return_value=True
    ), patch(
        "pystack.__main__.is_gzip", return_value=False
    ):
        get_process_threads_mock.return_value = threads
        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        Path("corefile"),
        Path("executable"),
        library_search_path="bar:foo",
        native_mode=NativeReportingMode.OFF,
        locals=False,
        method=StackMethod.AUTO,
    )
    TracebackPrinterMock.assert_called_once_with(NativeReportingMode.OFF)
    assert TracebackPrinterMock.return_value.print_thread.mock_calls == [
        call(thread) for thread in threads
    ]


def test_process_core_with_not_readable_search_root():
    # GIVEN

    argv = ["pystack", "core", "corefile", "executable", "--lib-search-root", "foo"]

    # WHEN

    with patch("pystack.__main__.get_process_threads_for_core"), patch(
        "pystack.__main__.TracebackPrinter"
    ), patch("sys.argv", argv), patch("pathlib.Path.exists", return_value=True), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ), patch(
        "os.path.isdir",
        return_value=True,
    ), patch(
        "os.access", return_value=False
    ), patch(
        "pystack.__main__.is_elf", return_value=True
    ):
        # THEN
        with pytest.raises(SystemExit):
            main()


def test_process_core_with_invalid_search_root():
    # GIVEN

    argv = ["pystack", "core", "corefile", "executable", "--lib-search-root", "foo"]

    # WHEN

    with patch("pystack.__main__.get_process_threads_for_core"), patch(
        "pystack.__main__.TracebackPrinter"
    ), patch("sys.argv", argv), patch("pathlib.Path.exists", return_value=True), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ), patch(
        "os.path.isdir",
        return_value=False,
    ):
        # THEN
        with pytest.raises(SystemExit):
            main()


def test_process_core_corefile_does_not_exit():
    # GIVEN

    argv = ["pystack", "core", "corefile", "executable"]

    # WHEN

    def path_exists(what):
        return what != Path("corefile")

    with patch(
        "pystack.__main__.get_process_threads_for_core"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ), patch.object(
        Path, "exists", path_exists
    ):
        # THEN

        with pytest.raises(SystemExit):
            main()

    # THEN

    get_process_threads_mock.assert_not_called()
    TracebackPrinterMock.assert_not_called()
    TracebackPrinterMock.return_value.print_thread.assert_not_called()


def test_process_core_executable_does_not_exit():
    # GIVEN

    argv = ["pystack", "core", "corefile", "executable"]

    # WHEN

    def does_exit(what):
        if what == Path("executable"):
            return False
        return True

    with patch(
        "pystack.__main__.get_process_threads_for_core"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "pystack.__main__.is_gzip", return_value=False
    ), patch(
        "sys.argv", argv
    ), patch.object(
        Path, "exists", does_exit
    ):
        # THEN
        with pytest.raises(SystemExit):
            main()

    # THEN

    get_process_threads_mock.assert_not_called()
    TracebackPrinterMock.assert_not_called()
    TracebackPrinterMock.return_value.print_thread.assert_not_called()


@pytest.mark.parametrize(
    "exception, exval", [(EngineError, 1), (InvalidPythonProcess, 2)]
)
def test_process_core_error(exception, exval, capsys):
    # GIVEN

    argv = ["pystack", "core", "corefile", "executable"]

    # WHEN

    with patch(
        "pystack.__main__.get_process_threads_for_core"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ), patch(
        "pystack.__main__.is_elf", return_value=True
    ), patch(
        "pystack.__main__.is_gzip", return_value=False
    ):
        # THEN
        get_process_threads_mock.side_effect = exception("Oh no!")
        with pytest.raises(SystemExit) as excinfo:
            main()
        assert excinfo.value.code == exval

    # THEN

    get_process_threads_mock.assert_called_once()
    TracebackPrinterMock.assert_called_once_with(NativeReportingMode.OFF)
    TracebackPrinterMock.return_value.print_thread.assert_not_called()
    capture = capsys.readouterr()
    assert "Oh no!" in capture.err


def test_process_core_exhaustive():
    # GIVEN

    argv = ["pystack", "core", "corefile", "executable", "--exhaustive"]

    threads = [Mock(), Mock(), Mock()]

    # WHEN
    with patch(
        "pystack.__main__.get_process_threads_for_core"
    ) as get_process_threads_mock, patch(
        "pystack.__main__.TracebackPrinter"
    ) as TracebackPrinterMock, patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ), patch(
        "pystack.__main__.is_elf", return_value=True
    ), patch(
        "pystack.__main__.is_gzip", return_value=False
    ):
        get_process_threads_mock.return_value = threads
        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        Path("corefile"),
        Path("executable"),
        library_search_path="",
        native_mode=NativeReportingMode.OFF,
        locals=False,
        method=StackMethod.ALL,
    )
    TracebackPrinterMock.assert_called_once_with(NativeReportingMode.OFF)
    assert TracebackPrinterMock.return_value.print_thread.mock_calls == [
        call(thread) for thread in threads
    ]


def test_default_colored_output():
    # GIVEN

    argv = ["pystack", "remote", "1234"]
    environ = {}

    # WHEN

    with patch("pystack.__main__.get_process_threads"), patch(
        "pystack.__main__.TracebackPrinter"
    ), patch("sys.argv", argv), patch("os.environ", environ):
        main()

    # THEN

    assert environ == {}


def test_nocolor_output():
    # GIVEN

    argv = ["pystack", "remote", "1234", "--no-color"]
    environ = {}

    # WHEN

    with patch("pystack.__main__.get_process_threads"), patch(
        "pystack.__main__.TracebackPrinter"
    ), patch("sys.argv", argv), patch("os.environ", environ):
        main()

    # THEN

    assert environ == {"NO_COLOR": "1"}


def test_nocolor_output_at_the_front_for_process():
    # GIVEN

    argv = ["pystack", "remote", "--no-color", "1234"]
    environ = {}

    # WHEN

    with patch("pystack.__main__.get_process_threads"), patch(
        "pystack.__main__.TracebackPrinter"
    ), patch("sys.argv", argv), patch("os.environ", environ):
        main()

    # THEN

    assert environ == {"NO_COLOR": "1"}


def test_nocolor_output_at_the_front_for_core():
    # GIVEN

    argv = ["pystack", "core", "--no-color", "corefile", "executable"]
    environ = {}

    # WHEN
    with patch("pystack.__main__.get_process_threads_for_core"), patch(
        "pystack.__main__.TracebackPrinter"
    ), patch("sys.argv", argv), patch("os.environ", environ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ), patch(
        "pystack.__main__.is_elf", return_value=True
    ), patch(
        "pystack.__main__.is_gzip", return_value=False
    ):
        main()

    # THEN

    assert environ == {"NO_COLOR": "1"}


@pytest.mark.parametrize("option", ["--no-color", "--verbose"])
def test_global_options_can_be_placed_at_any_point(option):
    # GIVEN

    argv = ["pystack", option, "core", option, "corefile", "executable"]
    environ = {}

    # WHEN
    with patch("pystack.__main__.get_process_threads_for_core"), patch(
        "pystack.__main__.TracebackPrinter"
    ), patch("sys.argv", argv), patch("os.environ", environ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ), patch(
        "pystack.__main__.is_elf", return_value=True
    ), patch(
        "pystack.__main__.is_gzip", return_value=False
    ):
        # THEN

        main()


def test_verbose_as_global_options_sets_correctly_the_logger():
    # GIVEN

    argv = ["pystack", "-vv", "remote", "1234"]
    environ = {}

    # WHEN
    with patch("pystack.__main__.get_process_threads"), patch(
        "pystack.__main__.TracebackPrinter"
    ), patch("sys.argv", argv), patch("os.environ", environ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ), patch(
        "pystack.__main__.logging.basicConfig"
    ) as logger_mock:
        # THEN

        main()

        logger_mock.assert_called_with(
            level=logging.DEBUG, format="%(levelname)s(%(funcName)s): %(message)s"
        )


def test_format_failureinfo_information_with_segfault():
    # GIVEN

    info = {
        "si_signo": 11,
        "si_errno": 0,
        "si_code": 1,
        "sender_pid": 0,
        "sender_uid": 0,
        "failed_addr": 123456789,
    }

    # WHEN
    with patch("os.environ", {"NO_COLOR": 1}):
        result = format_failureinfo_information(info)

    # THEN

    assert (
        result
        == "The process died due a segmentation fault accessing address: 0x75bcd15"
    )


def test_format_failureinfo_information_with_signal():
    # GIVEN
    info = {
        "si_signo": 7,
        "si_errno": 0,
        "si_code": 0,
        "sender_pid": 1,
        "sender_uid": 0,
        "failed_addr": 0,
    }

    # WHEN
    with patch("os.environ", {"NO_COLOR": 1}):
        result = format_failureinfo_information(info)

    # THEN

    assert result == "The process died due receiving signal SIGBUS sent by pid 1"


def test_format_failureinfo_information_with_signal_no_sender_pid():
    # GIVEN
    info = {
        "si_signo": 7,
        "si_errno": 0,
        "si_code": 0,
        "sender_uid": 0,
        "failed_addr": 0,
    }

    # WHEN
    with patch("os.environ", {"NO_COLOR": 1}):
        result = format_failureinfo_information(info)

    # THEN

    assert result == "The process died due receiving signal SIGBUS"


def test_format_failureinfo_information_with_no_info():
    # GIVEN
    info = {
        "si_signo": 0,
        "si_errno": 0,
        "si_code": 0,
        "sender_pid": 0,
        "sender_uid": 0,
        "failed_addr": 0,
    }

    # WHEN
    with patch("os.environ", {"NO_COLOR": 1}):
        result = format_failureinfo_information(info)

    # THEN

    assert (
        result
        == "The core file seems to have been generated on demand (the process did not crash)"
    )


def test_format_psinfo():
    # GIVEN
    info = {
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

    # WHEN
    with patch("os.environ", {"NO_COLOR": 1}):
        result = format_psinfo_information(info)

    # THEN

    expected = dedent(
        """\
    Core file information:
    state: R zombie: True niceness: 0
    pid: 75639 ppid: 1 sid: 1
    uid: 0 gid: 0 pgrp: 75639
    executable: a.out arguments: ./a.out
    """
    )

    assert expected.strip() == result.strip()


@pytest.mark.parametrize("method", ["extract_ps_info", "extract_failure_info"])
def test_process_core_does_not_crash_if_core_analyzer_fails(method):
    # GIVEN

    argv = ["pystack", "core", "corefile", "executable"]

    # WHEN / THEN

    with patch("pystack.__main__.get_process_threads_for_core"), patch(
        "pystack.__main__.TracebackPrinter"
    ), patch("pystack.__main__.is_elf", return_value=True), patch(
        "pystack.__main__.is_gzip", return_value=False
    ), patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ) as core_analyzer_test:
        method = getattr(core_analyzer_test(), method)
        method.side_effect = Exception("oh no")
        main()


@pytest.mark.parametrize("native", [True, False])
def test_core_file_missing_modules_are_logged(caplog, native):
    # GIVEN

    if native:
        argv = ["pystack", "core", "corefile", "executable", "--native"]
    else:
        argv = ["pystack", "core", "corefile", "executable"]

    # WHEN

    with patch("pystack.__main__.get_process_threads_for_core"), patch(
        "pystack.__main__.TracebackPrinter"
    ), patch("pystack.__main__.is_elf", return_value=True), patch(
        "pystack.__main__.is_gzip", return_value=False
    ), patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ) as core_analyzer_test:
        core_analyzer_test().missing_modules.return_value = ["A", "B"]
        with caplog.at_level(logging.WARNING):
            main()

    # THEN

    expected = ["Failed to locate A", "Failed to locate B"] if native else []
    record_messages = [record.message for record in caplog.records]
    assert record_messages == expected


@pytest.mark.parametrize("native", [True, False])
def test_core_file_missing_build_ids_are_logged(caplog, native):
    # GIVEN

    if native:
        argv = ["pystack", "core", "corefile", "executable", "--native"]
    else:
        argv = ["pystack", "core", "corefile", "executable"]

    # WHEN

    with patch("pystack.__main__.get_process_threads_for_core"), patch(
        "pystack.__main__.TracebackPrinter"
    ), patch("pystack.__main__.is_elf", return_value=True), patch(
        "pystack.__main__.is_gzip", return_value=False
    ), patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ) as core_analyzer_test:
        core_analyzer_test().extract_build_ids.return_value = [
            ("A", "1", "2"),
            ("B", "3", "4"),
            ("C", "5", "5"),
        ]
        with caplog.at_level(logging.WARNING):
            main()

    # THEN

    record_messages = [record.message for record in caplog.records]
    expected = (
        [
            "A needed BUILD ID 1 but found file with BUILD ID 2",
            "B needed BUILD ID 3 but found file with BUILD ID 4",
        ]
        if native
        else []
    )
    assert record_messages == expected


def test_executable_is_not_elf_uses_the_first_map():
    # GIVEN
    argv = ["pystack", "core", "corefile"]

    # WHEN
    real_executable = Path("/foo/bar/executable")

    with patch(
        "pystack.__main__.get_process_threads_for_core"
    ) as get_process_threads_mock, patch("pystack.__main__.TracebackPrinter"), patch(
        "pystack.__main__.is_elf", lambda x: x == real_executable
    ), patch(
        "pystack.__main__.is_gzip", return_value=False
    ), patch(
        "sys.argv", argv
    ), patch(
        "pathlib.Path.exists", return_value=True
    ), patch(
        "pystack.__main__.CoreFileAnalyzer"
    ) as core_analyzer_test:
        core_analyzer_test().extract_executable.return_value = "extracted_executable"
        core_analyzer_test().extract_maps.return_value = [
            VirtualMap(
                start=0x1000,
                end=0x2000,
                flags="r-xp",
                offset=0,
                device="00:00",
                inode=0,
                filesize=0,
                path=None,
            ),
            VirtualMap(
                start=0x2000,
                end=0x3000,
                flags="rw-p",
                offset=0,
                device="00:00",
                inode=0,
                filesize=0,
                path=Path("/foo/bar/executable"),
            ),
            VirtualMap(
                start=0x3000,
                end=0x4000,
                flags="r--p",
                offset=0,
                device="00:00",
                inode=0,
                filesize=0,
                path=None,
            ),
        ]
        main()

    # THEN

    get_process_threads_mock.assert_called_with(
        Path("corefile"),
        real_executable,
        library_search_path="",
        native_mode=NativeReportingMode.OFF,
        locals=False,
        method=StackMethod.AUTO,
    )
