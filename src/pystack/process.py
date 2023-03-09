import logging
import pathlib
import re
import subprocess
from typing import Optional
from typing import Tuple

from .errors import InvalidPythonProcess
from .maps import MemoryMapInformation
from .maps import VirtualMap

VERSION_REGEXP = re.compile(r"Python (?P<major>\d+)\.(?P<minor>\d+).*", re.IGNORECASE)

BINARY_REGEXP = re.compile(r"python(?P<major>\d+)\.(?P<minor>\d+).*", re.IGNORECASE)

LIBPYTHON_REGEXP = re.compile(
    r".*libpython(?P<major>\d+)\.(?P<minor>\d+).*", re.IGNORECASE
)

BSS_VERSION_REGEXP = re.compile(
    rb"((2|3)\.(\d+)\.(\d{1,2}))((a|b|c|rc)\d{1,2})?\+? (\(.{1,64}\))"
)

LOGGER = logging.getLogger(__file__)


def scan_process_bss_for_python_version(
    pid: int, bss: VirtualMap
) -> Optional[Tuple[int, int]]:
    # Lazy import _pystack to overcome a circular-import
    # (we really don't want a new extension just for this) :(
    try:
        from pystack._pystack import copy_memory_from_address
    except ImportError:  # pragma: no cover
        return None
    memory = copy_memory_from_address(pid, bss.start, bss.size)
    match = BSS_VERSION_REGEXP.findall(memory)
    if not match:
        return None
    ((_, major, minor, patch, *_),) = match
    return int(major), int(minor)


def scan_core_bss_for_python_version(
    corefile: pathlib.Path, bss: VirtualMap
) -> Optional[Tuple[int, int]]:
    with open(corefile, "rb") as the_corefile:
        the_corefile.seek(bss.offset)
        data = the_corefile.read(bss.size)
    match = next(BSS_VERSION_REGEXP.finditer(data), None)
    if not match:
        return None
    _, major, minor, patch, *_ = match.groups()
    return int(major), int(minor)


def _get_python_version_from_map_information(
    mapinfo: MemoryMapInformation,
) -> Tuple[int, int]:
    match = None
    assert mapinfo.python.path is not None
    if mapinfo.libpython:
        assert mapinfo.libpython.path is not None
        LOGGER.info(
            "Trying to extract version from filename: %s", mapinfo.libpython.path.name
        )
        match = LIBPYTHON_REGEXP.match(mapinfo.libpython.path.name)
    else:
        LOGGER.info(
            "Trying to extract version from filename: %s", mapinfo.python.path.name
        )
        match = BINARY_REGEXP.match(mapinfo.python.path.name)
    if match is None:
        LOGGER.info(
            "Could not find version by looking at library or binary path: "
            "Trying to get it from running python --version"
        )
        output = subprocess.check_output(
            [mapinfo.python.path, "--version"], text=True, stderr=subprocess.STDOUT
        )
        match = VERSION_REGEXP.match(output)
    if not match:
        raise InvalidPythonProcess(
            f"Could not determine python version from {mapinfo.python.path}"
        )
    major = match.group("major")
    minor = match.group("minor")
    LOGGER.info("Python version determined: %s.%s", major, minor)
    return int(major), int(minor)


def get_python_version_for_process(
    pid: int, mapinfo: MemoryMapInformation
) -> Tuple[int, int]:
    if mapinfo.bss is not None:
        version_from_bss = scan_process_bss_for_python_version(pid, mapinfo.bss)
        if version_from_bss is not None:
            LOGGER.info(
                "Version found by scanning the bss section: %d.%d", *version_from_bss
            )
            return version_from_bss

    return _get_python_version_from_map_information(mapinfo)


def get_python_version_for_core(
    corefile: pathlib.Path, executable: pathlib.Path, mapinfo: MemoryMapInformation
) -> Tuple[int, int]:
    if mapinfo.bss is not None:
        version_from_bss = scan_core_bss_for_python_version(corefile, mapinfo.bss)
        if version_from_bss is not None:
            LOGGER.info(
                "Version found by scanning the bss section: %d.%d", *version_from_bss
            )
            return version_from_bss
    return _get_python_version_from_map_information(mapinfo)


def is_elf(filename: pathlib.Path) -> bool:
    "Return True if the given file is an ELF file"
    elf_header = b"\x7fELF"
    with open(filename, "br") as thefile:
        return thefile.read(4) == elf_header


def get_thread_name(pid: int, tid: int) -> Optional[str]:
    try:
        with open(f"/proc/{pid}/task/{tid}/comm") as comm:
            return comm.read().strip()
    except OSError:
        return None
