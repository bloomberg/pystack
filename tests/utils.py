import collections
import contextlib
import itertools
import os
import pathlib
import shutil
import subprocess
import sys
import time
from typing import Generator
from typing import Iterable
from typing import List
from typing import Tuple

import pytest

from pystack._pystack import StackMethod
from pystack.errors import NotEnoughInformation

TIMEOUT = 30

PythonVersion = Tuple[Tuple[int, int], pathlib.Path]

ALL_VERSIONS = [
    ((3, 11), "python3.11"),
    ((3, 10), "python3.10"),
    ((3, 9), "python3.9"),
    ((3, 8), "python3.8"),
    ((3, 7), "python3.7"),
    ((3, 6), "python3.6"),
    ((2, 7), "python2.7"),
]

Interpreter = collections.namedtuple("Interpreter", "version path has_symbols")


def find_all_available_pythons() -> Iterable[Interpreter]:
    versions: List[Tuple[Tuple[int, int], str]]
    test_version = os.getenv("PYTHON_TEST_VERSION")
    if test_version == "auto":
        versions = [((sys.version_info[0], sys.version_info[1]), sys.executable)]
    elif test_version is not None:
        major, minor = test_version.split(".")
        versions = [((int(major), int(minor)), f"python{test_version}")]
    else:
        versions = ALL_VERSIONS

    for version, name in versions:
        location = shutil.which(name)
        if not location:
            continue

        result = subprocess.run(
            [location, "--version"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if result.returncode != 0:
            continue

        result = subprocess.run(
            ["file", "-L", location],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
        if result.returncode != 0:
            continue
        assert result.stdout is not None
        has_symbols = result.returncode == 0 and b" stripped" not in result.stdout

        yield Interpreter(version, pathlib.Path(location), has_symbols)


AVAILABLE_PYTHONS = tuple(find_all_available_pythons())


@contextlib.contextmanager
def spawn_child_process(
    python: str, test_file: str, tmpdir: pathlib.Path
) -> Generator["subprocess.Popen[str]", None, None]:
    fifo = tmpdir / "the_fifo"
    os.mkfifo(fifo)
    with subprocess.Popen(
        [python, test_file, str(fifo)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    ) as process:
        with open(fifo, "r") as fifo_file:
            response = fifo_file.read()

        assert response == "ready"
        time.sleep(0.1)
        try:
            yield process
        finally:
            os.remove(fifo)
            process.terminate()
            process.kill()
            process.wait(timeout=TIMEOUT)


@contextlib.contextmanager
def generate_core_file(
    python: pathlib.Path, test_file: pathlib.Path, tmpdir: pathlib.Path
) -> Generator[pathlib.Path, None, None]:
    fifo = tmpdir / "the_fifo"
    os.mkfifo(fifo)
    with subprocess.Popen(
        [python, test_file, str(fifo)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    ) as process:
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
        (core,) = pathlib.Path(tmpdir).glob("core.*")
        try:
            yield core
        finally:
            os.remove(fifo)
            process.terminate()
            process.kill()
            process.wait(timeout=TIMEOUT)


@contextlib.contextmanager
def xfail_on_expected_exceptions(
    method: StackMethod,
) -> Generator[None, None, None]:
    """Turn a NotEnoughInformation exception for the HEAP method into an XFAIL.

    The HEAP method is known to be flaky in some cases:

        - In Python 3.10 and earlier with the default glibc malloc,
          a PyThreadState should exist on the heap that points to the
          interpreter state.
        - For Python 3.11 and later, the PyThreadState for the main thread is
          statically allocated, so we would find a useful PyThreadState on the
          heap only for multithreaded programs.
        - For musl libc, its "mallocng" malloc uses the brk heap only for
          metadata, and always puts user data on anonymous mappings, so we'd
          never find a useful PyThreadstate on the heap segment.

    We want to keep running tests for the HEAP method to ensure it doesn't
    break in unexpected ways, but we'll just imperatively xfail if we see the
    failure condition that we know can happen.
    """
    try:
        yield
    except NotEnoughInformation:  # pragma: no cover
        if method == StackMethod.HEAP:
            pytest.xfail("Could not find interpreter state on brk heap")
        raise


def python_has_inlined_eval_frames(major: int, minor: int) -> bool:
    return (major, minor) >= (3, 11)


def python_has_position_information(major: int, minor: int) -> bool:
    return (major, minor) >= (3, 11)


def generate_all_pystack_combinations(
    corefile=False, native=False
) -> Iterable[Tuple[str, StackMethod, bool, Tuple[Tuple[int, int], pathlib.Path]]]:
    if corefile:
        stack_methods = (
            StackMethod.SYMBOLS,
            StackMethod.BSS,
            StackMethod.ELF_DATA,
            StackMethod.ANONYMOUS_MAPS,
        )
    else:
        stack_methods = (
            StackMethod.SYMBOLS,
            StackMethod.BSS,
            StackMethod.HEAP,
            StackMethod.ELF_DATA,
        )

    blocking_methods: Tuple[bool, ...]
    if native or corefile:
        blocking_methods = (True,)
    else:
        blocking_methods = (True, False)

    for method, blocking, python in itertools.product(
        stack_methods,
        blocking_methods,
        AVAILABLE_PYTHONS,
    ):
        (major_version, minor_version) = python.version
        if method == StackMethod.BSS and (
            major_version > 3 or (major_version == 3 and minor_version >= 10)
        ):
            continue
        if method == StackMethod.ELF_DATA and (
            major_version < 3 or (major_version == 3 and minor_version < 10)
        ):
            continue
        if method == StackMethod.HEAP and (
            major_version > 3 or (major_version == 3 and minor_version >= 11)
        ):
            continue
        if method == StackMethod.SYMBOLS and not python.has_symbols:
            continue

        python_id = "bbpy" if "bbpy" in python.path.name else ""
        the_id = (
            f"method={method.name}, blocking={blocking}, "
            f"python={python_id}{major_version}.{minor_version}"
        )

        yield the_id, method, blocking, python[:2]


def all_pystack_combinations(corefile=False, native=False):
    combinations = tuple(generate_all_pystack_combinations(corefile, native))

    test_combinations = tuple(
        (method, blocking, python) for _, method, blocking, python in combinations
    )
    ids = tuple(the_id for the_id, *_ in combinations)
    return pytest.mark.parametrize(
        "method, blocking, python", test_combinations, ids=ids
    )


ALL_PYTHONS = pytest.mark.parametrize(
    "python",
    [python[:2] for python in AVAILABLE_PYTHONS],
    ids=[python[1].name for python in AVAILABLE_PYTHONS],
)


ALL_PYTHONS_WITH_SYMBOLS = pytest.mark.parametrize(
    "python",
    [python[:2] for python in AVAILABLE_PYTHONS if python.has_symbols],
    ids=[python[1].name for python in AVAILABLE_PYTHONS if python.has_symbols],
)
