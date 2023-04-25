import argparse
import logging
import os
import pathlib
import signal
import sys
from contextlib import suppress
from textwrap import dedent
from typing import Any
from typing import Dict
from typing import NoReturn
from typing import Optional
from typing import Set

from pystack.errors import InvalidPythonProcess
from pystack.process import is_elf

from . import errors
from . import print_thread
from .colors import colored
from .engine import CoreFileAnalyzer
from .engine import NativeReportingMode
from .engine import StackMethod
from .engine import get_process_threads
from .engine import get_process_threads_for_core

PERMISSION_ERROR_MSG = "Operation not permitted"
NO_SUCH_PROCESS_ERROR_MSG = "No such process"

PERMISSION_HELP_TEXT = """
The specified process cannot be traced. This could be because the tracer
has insufficient privileges (the required capability is CAP_SYS_PTRACE).
Unprivileged processes cannot trace processes that they cannot send signals
to or those running set-user-ID/set-group-ID programs, for security reasons.
Alternatively, the process may already be being traced.

If your uid matches the uid of the target process you want to analyze, you
can do one of the following to get 'ptrace' scope permissions:

* If you are running inside a Docker container, you need to make sure you
  start the container using the '--cap-add=SYS_PTRACE' or '--privileged'
  command line arguments. Notice that this may not be enough if you are not
  running as 'root' inside the Docker container as you may need to disable
  hardening (see next points).

* Try running again with elevated permissions by running 'sudo -E !!'.

* You can disable kernel hardening for the current session temporarily (until
  a reboot happens) by running 'echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope'.
"""

LOGGER = logging.getLogger(__file__)


def _verbose_to_log_level(verbose_level: int) -> int:  # pragma: no cover
    if verbose_level == 0:
        return logging.WARNING
    elif verbose_level == 1:
        return logging.INFO
    else:
        return logging.DEBUG


def _exit_with_code(exception: BaseException) -> NoReturn:  # pragma: no cover
    if isinstance(exception, InvalidPythonProcess):
        sys.exit(2)
    sys.exit(1)


def produce_error_message(exception: BaseException) -> str:
    msg = (
        f"💀 {exception} 💀\n" if os.environ.get("NO_COLOR") is None else f"{exception}\n"
    )
    if PERMISSION_ERROR_MSG in str(exception) or isinstance(exception, PermissionError):
        msg += PERMISSION_HELP_TEXT
    if isinstance(exception, errors.PystackError) and hasattr(exception, "HELP_TEXT"):
        msg += getattr(exception, "HELP_TEXT")
    if (
        NO_SUCH_PROCESS_ERROR_MSG in str(exception)
        and isinstance(exception, errors.EngineError)
        and exception.pid is not None
        and pathlib.Path(f"/proc/{exception.pid}").exists()
    ):
        msg += PERMISSION_HELP_TEXT
    return colored(msg, "red")


class ReadableDirectory(argparse.Action):
    def __call__(
        self,
        parser: argparse.ArgumentParser,
        namespace: argparse.Namespace,
        values: Any,
        option_string: Optional[str] = None,
    ) -> None:
        prospective_dir = os.path.expanduser(values)
        if not os.path.isdir(prospective_dir):
            parser.error(f"{prospective_dir} is not a valid path")
        if not os.access(prospective_dir, os.R_OK):
            parser.error(f"{prospective_dir} is not a readable directory")
        setattr(namespace, self.dest, prospective_dir)


def generate_cli_parser() -> argparse.ArgumentParser:
    general_options_parser = argparse.ArgumentParser(add_help=False)
    general_options_parser.add_argument("-v", "--verbose", action="count", default=0)
    general_options_parser.add_argument(
        "--no-color",
        action="store_true",
        default=False,
        help="Deactivate colored output",
    )
    parser = argparse.ArgumentParser(
        description="Get python stack trace of a remote process",
    )
    parser.add_argument(
        "-v", "--verbose", action="count", default=0, dest="global_verbose"
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        dest="global_no_color",
        help="Deactivate colored output",
    )
    parser.add_argument_group("command")
    subparsers = parser.add_subparsers(
        title="commands",
        help="What should be analyzed by Pystack "
        "(use <command> --help for a command-specific help section).",
        dest="command",
    )
    subparsers.required = True
    remote_parser = subparsers.add_parser(
        "remote",
        help="Analyze a remote process given its PID",
        parents=[general_options_parser],
    )
    remote_parser.set_defaults(func=process_remote)
    remote_parser.add_argument("pid", type=int, help="The PID of the remote process")
    remote_parser.add_argument(
        "--no-block",
        dest="block",
        action="store_false",
        help="do not block the process when inspecting its memory",
    )
    remote_parser.add_argument(
        "--native",
        action="store_const",
        dest="native_mode",
        const=NativeReportingMode.PYTHON,
        default=NativeReportingMode.OFF,
        help="Include the native (C) frames in the resulting stack trace",
    )
    remote_parser.add_argument(
        "--native-all",
        action="store_const",
        dest="native_mode",
        const=NativeReportingMode.ALL,
        default=NativeReportingMode.OFF,
        help="Include native (C) frames from threads not registered with "
        "the interpreter (implies --native)",
    )
    remote_parser.add_argument(
        "--locals",
        action="store_true",
        default=False,
        help="Show local variables for each frame in the stack trace",
    )
    remote_parser.add_argument(
        "--exhaustive",
        action="store_true",
        default=False,
        help="Use all possible methods to obtain the Python stack info (may be slow)",
    )
    remote_parser.add_argument(
        "--self",
        action="store_true",
        default=False,
        help="Introspect the same process that invoke this program",
    )
    core_parser = subparsers.add_parser(
        "core",
        help="Analyze a core dump file given its location and the executable",
        parents=[general_options_parser],
    )
    core_parser.set_defaults(func=process_core)
    core_parser.add_argument(
        "core",
        type=str,
        help="The path to the core file",
    )
    core_parser.add_argument(
        "executable",
        help="(Optional) The path to the executable of the core file",
        nargs="?",
        default=None,
    )
    core_parser.add_argument(
        "--native",
        action="store_const",
        dest="native_mode",
        const=NativeReportingMode.PYTHON,
        default=NativeReportingMode.OFF,
        help="Include the native (C) frames in the resulting stack trace",
    )
    core_parser.add_argument(
        "--native-all",
        action="store_const",
        dest="native_mode",
        const=NativeReportingMode.ALL,
        default=NativeReportingMode.OFF,
        help="Include native (C) frames from threads not registered with "
        "the interpreter (implies --native)",
    )
    core_parser.add_argument(
        "--locals",
        action="store_true",
        default=False,
        help="Show local variables for each frame in the stack trace",
    )
    core_parser.add_argument(
        "--exhaustive",
        action="store_true",
        default=False,
        help="Use all possible methods to obtain the Python stack info (may be slow)",
    )
    search_path_group = core_parser.add_mutually_exclusive_group()
    search_path_group.add_argument(
        "--lib-search-path",
        dest="lib_search_path",
        default=None,
        help="List of paths to search for shared libraries loaded in the core. Paths must "
        "be separated by the ':' character",
    )
    search_path_group.add_argument(
        "--lib-search-root",
        dest="lib_search_root",
        action=ReadableDirectory,
        default=None,
        help="Root directory to search recursively for shared libraries loaded into the core.",
    )
    return parser


def main() -> None:
    parser = generate_cli_parser()
    args = parser.parse_args()
    args.no_color = args.no_color or args.global_no_color
    args.verbose = max(args.verbose, args.global_verbose)

    logging.basicConfig(
        level=_verbose_to_log_level(args.verbose),
        format="%(levelname)s(%(funcName)s): %(message)s",
    )

    if args.no_color:
        os.environ["NO_COLOR"] = "1"

    try:
        args.func(parser, args)
    except (errors.PystackError, PermissionError) as the_error:
        print(produce_error_message(the_error), file=sys.stderr)
        _exit_with_code(the_error)


def process_remote(parser: argparse.ArgumentParser, args: argparse.Namespace) -> None:
    if not args.block and args.native_mode != NativeReportingMode.OFF:
        parser.error("Native traces are only available in blocking mode")

    for thread in get_process_threads(
        args.pid,
        stop_process=args.block,
        native_mode=args.native_mode,
        locals=args.locals,
        method=StackMethod.ALL if args.exhaustive else StackMethod.AUTO,
    ):
        native = args.native_mode != NativeReportingMode.OFF
        print_thread(thread, native)


def format_psinfo_information(psinfo: Dict[str, Any]) -> str:
    def as_green_str(obj: Any) -> str:
        return colored(str(obj), "green")

    def green(key: str) -> str:
        return as_green_str(psinfo.get(key))

    state = as_green_str(chr(psinfo["sname"]))
    zombie = as_green_str(bool([psinfo["zomb"]]))
    return dedent(
        f"""
    {colored("Core file information:", "blue")}
    state: {state} zombie: {zombie} niceness: {green('nice')}
    pid: {green('pid')} ppid: {green('ppid')} sid: {green('sid')}
    uid: {green('uid')} gid: {green('gid')} pgrp: {green('pgrp')}
    executable: {green('fname')} arguments: {green('psargs')}
    """
    )


def format_failureinfo_information(failure_info: Dict[str, Any]) -> str:
    def as_yellow_str(obj: Any) -> str:
        return colored(str(obj), "yellow")

    if failure_info["failed_addr"] != 0:
        return (
            f"{as_yellow_str('The process died due a segmentation fault accessing address: ')}"
            f"{colored(str(hex(failure_info['failed_addr'])), 'red')}"
        )
    elif failure_info["si_signo"]:
        signame = colored(signal.Signals(failure_info["si_signo"]).name, "red")
        msg = f"The process died due receiving signal {signame}"
        if failure_info.get("sender_pid"):
            sender = colored(str(failure_info["sender_pid"]), "red")
            msg += as_yellow_str(f" sent by pid {sender}")
        return as_yellow_str(msg)
    else:
        return as_yellow_str(
            "The core file seems to have been generated on demand (the process did not crash)"
        )


def process_core(parser: argparse.ArgumentParser, args: argparse.Namespace) -> None:
    corefile = pathlib.Path(args.core)
    if not corefile.exists():
        parser.error(f"Core {corefile} does not exist")

    if args.executable is None:
        corefile_analyzer = CoreFileAnalyzer(corefile)
        executable = pathlib.Path(corefile_analyzer.extract_executable())
        if not executable.exists():
            raise errors.DetectedExecutableNotFound(
                f"Detected executable doesn't exist: {executable}"
            )
        print(f"Using executable found in the core file: {executable}")
    else:
        executable = pathlib.Path(args.executable)

    if not executable.exists():
        parser.error(f"Executable {executable} does not exist")

    lib_search_path = ""
    if args.lib_search_path:
        lib_search_path = args.lib_search_path
    if args.lib_search_root:
        library_dirs: Set[str] = set()
        for pattern in {"**/*.so", "**/*.so.*"}:
            library_dirs.update(
                str(file.parent)
                for file in pathlib.Path(args.lib_search_root).glob(pattern)
            )
        lib_search_path = ":".join(sorted(library_dirs))
    LOGGER.info("Using library search path: %s", lib_search_path)

    corefile_analyzer = CoreFileAnalyzer(corefile, executable, lib_search_path)
    with suppress(Exception):
        print(format_psinfo_information(corefile_analyzer.extract_ps_info()))
        print(format_failureinfo_information(corefile_analyzer.extract_failure_info()))

    if not is_elf(executable):
        raise errors.InvalidExecutable(
            f"The provided executable ({executable}) doesn't have an executable format"
        )

    if args.native_mode != NativeReportingMode.OFF:
        for module in corefile_analyzer.missing_modules():
            LOGGER.warning("Failed to locate %s", module)
        for filename, core_id, elf_id in corefile_analyzer.extract_build_ids():
            if not pathlib.Path(filename).exists() or not core_id or core_id == elf_id:
                continue
            LOGGER.warning(
                "%s needed BUILD ID %s but found file with BUILD ID %s",
                filename,
                core_id if core_id else "<MISSING>",
                elf_id if elf_id else "<MISSING>",
            )

    for thread in get_process_threads_for_core(
        corefile,
        executable,
        library_search_path=lib_search_path,
        native_mode=args.native_mode,
        locals=args.locals,
        method=StackMethod.ALL if args.exhaustive else StackMethod.AUTO,
    ):
        native = args.native_mode != NativeReportingMode.OFF
        print_thread(thread, native)


if __name__ == "__main__":  # pragma: no cover
    main()
