from unittest.mock import patch, Mock
from pystack.__main__ import format_failureinfo_information, format_psinfo_information, main, produce_error_message
from pystack.errors import EngineError, InvalidPythonProcess
import pytest
from pathlib import Path

RANGE=100

def time_format_failureinfo_information_with_segfault():
    
    for i in range(RANGE):
        info = {
            "si_signo": 11,
            "si_errno": 0,
            "si_code": 1,
            "sender_pid": 0,
            "sender_uid": 0,
            "failed_addr": 123456789,
        }

        with patch("os.environ", {"NO_COLOR": 1}):
            result = format_failureinfo_information(info)

def time_format_failureinfo_information_with_signal():
    for i in range(RANGE):
        info = {
            "si_signo": 7,
            "si_errno": 0,
            "si_code": 0,
            "sender_pid": 1,
            "sender_uid": 0,
            "failed_addr": 0,
        }

        with patch("os.environ", {"NO_COLOR": 1}):
            result = format_failureinfo_information(info)

def time_format_failureinfo_information_with_signal_no_sender_pid():
    for i in range(RANGE):
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

def time_format_failureinfo_information_with_no_info():
    for i in range(RANGE):
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

def time_format_psinfo():
    for i in range(RANGE):
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

def time_process_remote_default():
    for i in range(RANGE):
        argv = ["pystack", "remote", "31"]

        threads = [Mock(), Mock(), Mock()]

        # WHEN

        with patch(
            "pystack.__main__.get_process_threads"
        ) as get_process_threads_mock, patch(
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
            "sys.argv", argv
        ):
            get_process_threads_mock.return_value = threads
            main()

def time_process_remote_no_block():
    for i in range(RANGE):
        argv = ["pystack", "remote", "31", "--no-block"]

        threads = [Mock(), Mock(), Mock()]

        with patch(
            "pystack.__main__.get_process_threads"
        ) as get_process_threads_mock, patch(
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
            "sys.argv", argv
        ):
            get_process_threads_mock.return_value = threads
            main()

def time_process_remote_native():
    for i in range(RANGE):
        argv = ["pystack", "remote", "31", "--native"]

        threads = [Mock(), Mock(), Mock()]

        with patch(
            "pystack.__main__.get_process_threads"
        ) as get_process_threads_mock, patch(
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
            "sys.argv", argv
        ):
            get_process_threads_mock.return_value = threads
            main()

def time_process_remote_locals():
    for i in range(RANGE):
        argv = ["pystack", "remote", "31", "--locals"]

        threads = [Mock(), Mock(), Mock()]

        with patch(
            "pystack.__main__.get_process_threads"
        ) as get_process_threads_mock, patch(
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
            "sys.argv", argv
        ):
            get_process_threads_mock.return_value = threads
            main()

def time_process_remote_native_no_block():
    for i in range(RANGE):
        argv = ["pystack", "remote", "31", "--native","--no-block"]

        threads = [Mock(), Mock(), Mock()]

        with patch(
            "pystack.__main__.get_process_threads"
        ) as get_process_threads_mock, patch(
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
            "sys.argv", argv
        ):
            get_process_threads_mock.return_value = threads
            with pytest.raises(SystemExit):
                main()

def time_process_remote_exhaustive():
    for i in range(RANGE):
        argv = ["pystack", "remote", "31", "--exhaustive"]

        threads = [Mock(), Mock(), Mock()]

        with patch(
            "pystack.__main__.get_process_threads"
        ) as get_process_threads_mock, patch(
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
            "sys.argv", argv
        ):
            get_process_threads_mock.return_value = threads
            main()

def time_process_remote_error():
    for i in range(RANGE):
        for exception in [EngineError, InvalidPythonProcess]:
            argv = ["pystack", "remote", "32"]

            with patch(
                "pystack.__main__.get_process_threads"
            ) as get_process_threads_mock, patch(
                "pystack.__main__.print_thread"
            ) as print_thread_mock, patch(
                "sys.argv", argv
            ), patch(
                "pathlib.Path.exists", return_value=True
            ):
                get_process_threads_mock.side_effect = exception("Oh no!")
                with pytest.raises(SystemExit) as excinfo:
                    main()

def time_process_core_defaulte_without_executable():
    for i in range(RANGE):
        argv = ["pystack", "core", "corefile"]

        threads = [Mock(), Mock(), Mock()]

        # WHEN

        with patch(
            "pystack.__main__.get_process_threads_for_core"
        ) as get_process_threads_mock, patch(
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
            "sys.argv", argv
        ), patch(
            "pathlib.Path.exists", return_value=True
        ), patch(
            "pystack.__main__.is_elf", return_value=True
        ), patch(
            "pystack.__main__.CoreFileAnalyzer"
        ) as core_file_analizer_mock:
            core_file_analizer_mock().extract_executable.return_value = (
                "extracted_executable"
            )
            get_process_threads_mock.return_value = threads
            main()

def time_process_core_default_without_executable_and_executable_does_not_exist():
    for i in range(RANGE):
        argv = ["pystack", "core", "corefile"]

        # WHEN

        with patch("sys.argv", argv), patch(
            "pathlib.Path.exists"
        ) as path_exists_mock, patch(
            "pystack.__main__.CoreFileAnalyzer"
        ) as core_file_analizer_mock:
            core_file_analizer_mock().extract_executable.return_value = (
                "extracted_executable"
            )
            # THEN
            path_exists_mock.side_effect = [True, False]

            with pytest.raises(SystemExit):
                main()

def time_process_core_executable_not_elf_file():
    # GIVEN
    for i in range(RANGE):
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
        ):
            get_process_threads_mock.return_value = threads
            with pytest.raises(SystemExit):
                main()

def time_process_core_default_with_executable():
    for i in range(RANGE):
        argv = ["pystack", "core", "corefile", "executable"]

        threads = [Mock(), Mock(), Mock()]

        # WHEN

        with patch(
            "pystack.__main__.get_process_threads_for_core"
        ) as get_process_threads_mock, patch(
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
            "sys.argv", argv
        ), patch(
            "pathlib.Path.exists", return_value=True
        ), patch(
            "pystack.__main__.CoreFileAnalyzer"
        ), patch(
            "pystack.__main__.is_elf", return_value=True
        ):
            get_process_threads_mock.return_value = threads
            main()

def time_process_core_native():
    # GIVEN
    for i in range(RANGE):
        for argument in ['--native','--native-all']:
            argv = ["pystack", "core", "corefile", "executable", argument]

            threads = [Mock(), Mock(), Mock()]

            # WHEN

            with patch(
                "pystack.__main__.get_process_threads_for_core"
            ) as get_process_threads_mock, patch(
                "pystack.__main__.print_thread"
            ) as print_thread_mock, patch(
                "sys.argv", argv
            ), patch(
                "pathlib.Path.exists", return_value=True
            ), patch(
                "pystack.__main__.CoreFileAnalyzer"
            ), patch(
                "pystack.__main__.is_elf", return_value=True
            ):
                get_process_threads_mock.return_value = threads
                main()

def time_process_core_locals():
    for i in range(RANGE):
        argv = ["pystack", "core", "corefile", "executable", "--locals"]

        threads = [Mock(), Mock(), Mock()]

        # WHEN

        with patch(
            "pystack.__main__.get_process_threads_for_core"
        ) as get_process_threads_mock, patch(
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
            "sys.argv", argv
        ), patch(
            "pathlib.Path.exists", return_value=True
        ), patch(
            "pystack.__main__.CoreFileAnalyzer"
        ), patch(
            "pystack.__main__.is_elf", return_value=True
        ):
            get_process_threads_mock.return_value = threads
            main()


def time_process_core_with_search_path():
    # GIVEN
    for i in range(RANGE):
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
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
            "sys.argv", argv
        ), patch(
            "pathlib.Path.exists", return_value=True
        ), patch(
            "pystack.__main__.CoreFileAnalyzer"
        ), patch(
            "pystack.__main__.is_elf", return_value=True
        ):
            get_process_threads_mock.return_value = threads
            main()

def time_process_core_with_search_root():
    # GIVEN
    for i in range(RANGE):
        argv = ["pystack", "core", "corefile", "executable", "--lib-search-root", "foo"]

        threads = [Mock(), Mock(), Mock()]

        # WHEN

        with patch(
            "pystack.__main__.get_process_threads_for_core"
        ) as get_process_threads_mock, patch(
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
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
        ):
            get_process_threads_mock.return_value = threads
            main()

def time_process_core_with_not_readable_search_root():
    # GIVEN
    for i in range(RANGE):
        argv = ["pystack", "core", "corefile", "executable", "--lib-search-root", "foo"]

        # WHEN

        with patch("pystack.__main__.get_process_threads_for_core"), patch(
            "pystack.__main__.print_thread"
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

def time_process_core_with_invalid_search_root():
    # GIVEN
    for i in range(RANGE):
        argv = ["pystack", "core", "corefile", "executable", "--lib-search-root", "foo"]

        # WHEN

        with patch("pystack.__main__.get_process_threads_for_core"), patch(
            "pystack.__main__.print_thread"
        ), patch("sys.argv", argv), patch("pathlib.Path.exists", return_value=True), patch(
            "pystack.__main__.CoreFileAnalyzer"
        ), patch(
            "os.path.isdir",
            return_value=False,
        ):
            # THEN
            with pytest.raises(SystemExit):
                main()  

def time_process_core_corefile_does_not_exit():
    # GIVEN
    for i in range(RANGE):
        argv = ["pystack", "core", "corefile", "executable"]

        # WHEN

        def path_exists(what):
            return what != Path("corefile")

        with patch(
            "pystack.__main__.get_process_threads_for_core"
        ) as get_process_threads_mock, patch(
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
            "sys.argv", argv
        ), patch.object(
            Path, "exists", path_exists
        ):
            # THEN

            with pytest.raises(SystemExit):
                main()  

def time_process_core_executable_does_not_exit():
    # GIVEN
    for i in range(RANGE):
        argv = ["pystack", "core", "corefile", "executable"]

        # WHEN

        def does_exit(what):
            if what == Path("executable"):
                return False
            return True

        with patch(
            "pystack.__main__.get_process_threads_for_core"
        ) as get_process_threads_mock, patch(
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
            "sys.argv", argv
        ), patch.object(
            Path, "exists", does_exit
        ):
            # THEN
            with pytest.raises(SystemExit):
                main()

def time_process_core_error():
    # GIVEN
    for i in range(RANGE):
        for exception in [EngineError, InvalidPythonProcess]:
            argv = ["pystack", "core", "corefile", "executable"]

            # WHEN

            with patch(
                "pystack.__main__.get_process_threads_for_core"
            ) as get_process_threads_mock, patch(
                "pystack.__main__.print_thread"
            ) as print_thread_mock, patch(
                "sys.argv", argv
            ), patch(
                "pathlib.Path.exists", return_value=True
            ), patch(
                "pystack.__main__.CoreFileAnalyzer"
            ), patch(
                "pystack.__main__.is_elf", return_value=True
            ):
                # THEN
                get_process_threads_mock.side_effect = exception("Oh no!")
                with pytest.raises(SystemExit) as excinfo:
                    main()  

def time_process_core_exhaustive():
    # GIVEN
    for i in range(RANGE):
        argv = ["pystack", "core", "corefile", "executable", "--exhaustive"]

        threads = [Mock(), Mock(), Mock()]

        # WHEN
        with patch(
            "pystack.__main__.get_process_threads_for_core"
        ) as get_process_threads_mock, patch(
            "pystack.__main__.print_thread"
        ) as print_thread_mock, patch(
            "sys.argv", argv
        ), patch(
            "pathlib.Path.exists", return_value=True
        ), patch(
            "pystack.__main__.CoreFileAnalyzer"
        ), patch(
            "pystack.__main__.is_elf", return_value=True
        ):
            get_process_threads_mock.return_value = threads
            main()

def time_default_colored_output():
    # GIVEN
    for i in range(RANGE):
        argv = ["pystack", "remote", "1234"]
        environ = {}

        # WHEN

        with patch("pystack.__main__.get_process_threads"), patch(
            "pystack.__main__.print_thread"
        ), patch("sys.argv", argv), patch("os.environ", environ):
            main()

def time_nocolor_output():
    # GIVEN
    for i in range(RANGE):
        argv = ["pystack", "remote", "1234", "--no-color"]
        environ = {}

        # WHEN

        with patch("pystack.__main__.get_process_threads"), patch(
            "pystack.__main__.print_thread"
        ), patch("sys.argv", argv), patch("os.environ", environ):
            main()

def time_nocolor_output_at_the_front_for_process():
    # GIVEN
    for i in range(RANGE):
        argv = ["pystack", "remote", "--no-color", "1234"]
        environ = {}

        # WHEN

        with patch("pystack.__main__.get_process_threads"), patch(
            "pystack.__main__.print_thread"
        ), patch("sys.argv", argv), patch("os.environ", environ):
            main()

def time_nocolor_output_at_the_front_for_core():
    # GIVEN
    for i in range(RANGE):
        argv = ["pystack", "core", "--no-color", "corefile", "executable"]
        environ = {}

        # WHEN
        with patch("pystack.__main__.get_process_threads_for_core"), patch(
            "pystack.__main__.print_thread"
        ), patch("sys.argv", argv), patch("os.environ", environ), patch(
            "pathlib.Path.exists", return_value=True
        ), patch(
            "pystack.__main__.CoreFileAnalyzer"
        ), patch(
            "pystack.__main__.is_elf", return_value=True
        ):
            main()

def test_global_options_can_be_placed_at_any_point():
    # GIVEN
    for i in range(RANGE):
        for option in ['--no-color','--verbose']:
            argv = ["pystack", option, "core", option, "corefile", "executable"]
            environ = {}

            # WHEN
            with patch("pystack.__main__.get_process_threads_for_core"), patch(
                "pystack.__main__.print_thread"
            ), patch("sys.argv", argv), patch("os.environ", environ), patch(
                "pathlib.Path.exists", return_value=True
            ), patch(
                "pystack.__main__.CoreFileAnalyzer"
            ), patch(
                "pystack.__main__.is_elf", return_value=True
            ):
                # THEN

                main()

def time_verbose_as_global_options_sets_correctly_the_logger():
    # GIVEN
    for i in range(RANGE):
        argv = ["pystack", "-vv", "remote", "1234"]
        environ = {}

        # WHEN
        with patch("pystack.__main__.get_process_threads"), patch(
            "pystack.__main__.print_thread"
        ), patch("sys.argv", argv), patch("os.environ", environ), patch(
            "pathlib.Path.exists", return_value=True
        ), patch(
            "pystack.__main__.CoreFileAnalyzer"
        ), patch(
            "pystack.__main__.logging.basicConfig"
        ) as logger_mock:
            # THEN

            main()

def time_process_core_does_not_crash_if_core_analyzer_fails():
    # GIVEN
    for i in range(RANGE):
        for method in ["extract_ps_info", "extract_failure_info"]:
            argv = ["pystack", "core", "corefile", "executable"]

            # WHEN / THEN

            with patch("pystack.__main__.get_process_threads_for_core"), patch(
                "pystack.__main__.print_thread"
            ), patch("pystack.__main__.is_elf", return_value=True), patch(
                "sys.argv", argv
            ), patch(
                "pathlib.Path.exists", return_value=True
            ), patch(
                "pystack.__main__.CoreFileAnalyzer"
            ) as core_analyzer_test:
                method = getattr(core_analyzer_test(), method)
                method.side_effect = Exception("oh no")
                main()  
