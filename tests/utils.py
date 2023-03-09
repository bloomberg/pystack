import contextlib
import itertools
import os
import pathlib
import shutil
import subprocess
import time
from typing import Generator
from typing import Iterable
from typing import Tuple

import pytest

from pystack._pystack import StackMethod

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


def find_all_available_pythons() -> Iterable[pathlib.Path]:
    test_version = os.getenv("PYTHON_TEST_VERSION")
    if test_version is not None:
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
        yield (version, pathlib.Path(location))


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


def python_has_inlined_eval_frames(major: int, minor: int) -> bool:
    if major > 3 or (major == 3 and minor >= 11):
        return True
    return False


def python_has_position_information(major: int, minor: int) -> bool:
    return (major, minor) >= (3, 11)


def generate_all_pystack_combinations(
    corefile=False, native=False
) -> Tuple[str, StackMethod, bool, pathlib.Path]:
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

    if native or corefile:
        blocking_methods = (True,)
    else:
        blocking_methods = (True, False)

    for method, blocking, python in itertools.product(
        stack_methods,
        blocking_methods,
        AVAILABLE_PYTHONS,
    ):
        ((major_version, minor_version), _) = python
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

        python_id = "bbpy" if "bbpy" in python[1].name else ""
        the_id = (
            f"method={method.name}, blocking={blocking}, "
            f"python={python_id}{major_version}.{minor_version}"
        )

        yield the_id, method, blocking, python


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
    "python", AVAILABLE_PYTHONS, ids=[python[1].name for python in AVAILABLE_PYTHONS]
)
