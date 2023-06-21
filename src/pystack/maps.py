import collections
import dataclasses
import logging
import os
import re
from pathlib import Path
from typing import Any
from typing import Dict
from typing import Iterable
from typing import List
from typing import Optional

from .errors import MissingExecutableMaps
from .errors import ProcessNotFound
from .errors import PystackError

LOGGER = logging.getLogger(__file__)

MAPS_REGEXP = re.compile(
    r"""
    (?P<start>[\da-f]+)
    -
    (?P<end>[\da-f]+)
    \s
    (?P<permissions>....)
    \s
    (?P<offset>[\da-f]+)
    \s
    (?P<dev>[\da-f][\da-f]+:[\da-f][\da-f]+)
    \s
    (?P<inode>\d+)
    \s*
    (?P<pathname>.+)?
    $
    """,
    re.VERBOSE,
)

RawCoreMapList = List[Dict[str, Any]]


@dataclasses.dataclass(frozen=True, eq=True)
class VirtualMap:
    start: int
    end: int
    filesize: int
    offset: int
    device: str
    flags: str
    inode: int
    path: Optional[Path]

    def contains(self, addr: int) -> bool:
        return self.start <= addr < self.end

    def is_executable(self) -> bool:
        return "x" in self.flags

    def is_readable(self) -> bool:
        return "r" in self.flags

    def is_writable(self) -> bool:
        return "w" in self.flags

    def is_private(self) -> bool:
        return "p" in self.flags

    @property
    def size(self) -> int:
        return self.end - self.start

    def __repr__(self) -> str:
        start = f"0x{self.start:016x}"
        end = f"0x{self.end:016x}"
        filesize = f"0x{self.filesize:x}"
        offset = f"0x{self.offset:x}"
        return (
            f"VirtualMap(start={start!s}, end={end!s},"
            f" filesize={filesize!s}, offset={offset!s},"
            f" device={self.device!r}, flags={self.flags!r}, inode={self.inode!r},"
            f" path={str(self.path)!r})"
        )


@dataclasses.dataclass
class MemoryRange:
    min_addr: int
    max_addr: int


@dataclasses.dataclass
class MemoryMapInformation:
    memory: MemoryRange
    heap: Optional[VirtualMap]
    bss: Optional[VirtualMap]
    python: VirtualMap
    libpython: Optional[VirtualMap]


def _read_maps(pid: int) -> List[str]:
    try:
        with open(f"/proc/{pid}/maps") as maps:
            return maps.readlines()
    except FileNotFoundError:
        raise ProcessNotFound(f"No such process id: {pid}") from None


def generate_maps_for_process(pid: int) -> Iterable[VirtualMap]:
    proc_maps_lines = _read_maps(pid)
    for index, line in enumerate(proc_maps_lines):
        line = line.rstrip("\n")
        match = MAPS_REGEXP.match(line)
        if not match:
            LOGGER.debug("Line %r cannot be recognized!", line)
            continue

        path = match.group("pathname")
        yield VirtualMap(
            start=int(match.group("start"), 16),
            end=int(match.group("end"), 16),
            filesize=int(match.group("end"), 16) - int(match.group("start"), 16),
            offset=int(match.group("offset"), 16),
            device=match.group("dev"),
            flags=match.group("permissions"),
            inode=int(match.group("inode")),
            path=Path(path) if path else None,
        )


def generate_maps_from_core_data(
    mapped_files: RawCoreMapList, memory_maps: RawCoreMapList
) -> Iterable[VirtualMap]:
    memory_map_ranges = {(map["start"], map["end"]) for map in memory_maps}
    missing_mapped_files = [
        map
        for map in mapped_files
        if (map["start"], map["end"]) not in memory_map_ranges
    ]

    all_maps: RawCoreMapList = sorted(
        memory_maps + missing_mapped_files, key=lambda map: map["start"]
    )

    # Some paths in the mapped files can be absolute, but we need to work with the canonical
    # paths that the linker reported, so we need to "unresolve" those path back to whatever
    # the memory math paths are so we can properly group then together. For example, the map
    # for the interpreter may be "/usr/bin/python" in the mapped files and "/venv/bin/python"
    # in the memory maps.
    missing_map_paths = {
        Path(map["path"]) for map in missing_mapped_files if map is not None
    }
    file_maps = {}
    for map in memory_maps:
        if not map["path"]:
            continue
        the_path = Path(map["path"])
        resolved_path = the_path.resolve()
        if resolved_path in missing_map_paths:
            file_maps[resolved_path] = the_path

    for data_elem in all_maps:
        path = Path(data_elem["path"]) if data_elem["path"] else None
        if path is not None:
            path = file_maps.get(path, path)

        yield VirtualMap(
            start=data_elem["start"],
            end=data_elem["end"],
            filesize=data_elem["filesize"],
            offset=data_elem["offset"],
            device=data_elem["device"],
            flags=data_elem["flags"],
            inode=data_elem["inode"],
            path=path,
        )


def parse_maps_file(pid: int, all_maps: Iterable[VirtualMap]) -> MemoryMapInformation:
    binary_name = Path(os.readlink(f"/proc/{pid}/exe"))
    return parse_maps_file_for_binary(binary_name, all_maps)


def _get_base_map(binary_maps: List[VirtualMap]) -> VirtualMap:
    maybe_map = next(
        (map for map in binary_maps if map.path is not None),
        None,
    )
    if maybe_map is not None:
        return maybe_map
    first_map, *_ = binary_maps
    return first_map


def _get_bss(elf_maps: List[VirtualMap], load_point: int) -> Optional[VirtualMap]:
    binary_map = _get_base_map(elf_maps)
    if not binary_map or not binary_map.path:
        return None
    try:
        from ._pystack import get_bss_info
    except ImportError:  # pragma: no cover
        return None
    bss_info = get_bss_info(binary_map.path)
    if not bss_info:
        return None
    start = load_point + bss_info["corrected_addr"]
    LOGGER.info(
        "Determined exact addr of .bss section: %s (%s + %s)",
        hex(start),
        hex(load_point),
        hex(bss_info["corrected_addr"]),
    )
    offset = 0

    # Calculate the offset based on the mapped files. The offset in core files
    # is only present in the core (and not in the original ELF) so this
    # operation allows us to correlate the bss section with some memory location
    # within the core file.
    first_matching_map = next((map for map in elf_maps if map.contains(start)), None)
    if first_matching_map is None:
        return None

    offset = first_matching_map.offset + (start - first_matching_map.start)

    bss = VirtualMap(
        start=start,
        end=start + bss_info["size"],
        filesize=bss_info["size"],
        offset=offset,
        device="",
        flags="",
        inode=0,
        path=None,
    )
    return bss


def parse_maps_file_for_binary(
    binary_name: Path,
    all_maps_iter: Iterable[VirtualMap],
    load_point_by_module: Optional[Dict[str, int]] = None,
) -> MemoryMapInformation:
    min_addr = float("inf")
    max_addr = 0
    maps_by_library: Dict[str, List[VirtualMap]] = collections.defaultdict(list)
    current_lib = ""
    all_maps = tuple(all_maps_iter)

    if load_point_by_module is None:
        load_point_by_module = collections.defaultdict(lambda: 2**64)
        for memory_range in all_maps:
            if memory_range.path is not None:
                load_point_by_module[memory_range.path.name] = min(
                    memory_range.start,
                    load_point_by_module[memory_range.path.name],
                )

    for memory_range in all_maps:
        current_lib = (
            memory_range.path.name if memory_range.path is not None else current_lib
        )
        maps_by_library[current_lib].append(memory_range)

        if memory_range.path is None or not memory_range.path.name.startswith("[v"):
            min_addr = min(min_addr, memory_range.start)
            max_addr = max(max_addr, memory_range.end)
    maps_by_library = dict(maps_by_library)

    python = libpython = bss = heap = None
    try:
        binary_maps = maps_by_library[binary_name.name]
        python = _get_base_map(binary_maps)
    except KeyError:
        LOGGER.debug("Unable to find maps for %r in %r", binary_name, maps_by_library)
        available_maps = {
            str(map.path)
            for map in all_maps
            if map.path is not None and ".so" not in map.path.name
        }
        LOGGER.debug("Available executable maps: %s", ", ".join(available_maps))
        if available_maps:
            maps_txt = ", ".join(available_maps)
            msg = f"These are the available executable memory maps: {maps_txt}"
        else:
            msg = "There are no available executable maps with known paths."
        raise MissingExecutableMaps(
            f"Unable to find maps for the executable {binary_name}. " + msg
        )
    LOGGER.info("python binary first map found: %r", python)

    libpython_binaries = [lib for lib in maps_by_library if "libpython" in lib]
    if len(libpython_binaries) > 1:
        raise PystackError(
            f"Unexpectedly found multiple libpython in process: {libpython_binaries}"
        )
    elif len(libpython_binaries) == 1:
        libpython_name = libpython_binaries[0]
        libpython_maps = maps_by_library[libpython_name]
        load_point = load_point_by_module[libpython_name]
        elf_maps = libpython_maps
        libpython = _get_base_map(libpython_maps)
        LOGGER.info("%r first map found: %r", libpython_name, libpython)
    else:
        LOGGER.info("Process does not have a libpython.so, reading from binary")
        elf_maps = binary_maps
        libpython = None
        load_point = load_point_by_module[binary_name.name]

    heap_maps = maps_by_library.get("[heap]")
    if heap_maps is not None:
        *_, heap = [m for m in heap_maps if getattr(m.path, "name", None) == "[heap]"]
        LOGGER.info("Heap map found: %r", heap)

    bss = _get_bss(elf_maps, load_point)
    if bss is None:
        bss = (
            next(
                (map for map in elf_maps if map.path is None and map.is_readable()),
                None,
            )
            if elf_maps
            else None
        )
    if bss:
        LOGGER.info("bss map found: %r", bss)

    memory = MemoryRange(min_addr=int(min_addr), max_addr=int(max_addr))
    return MemoryMapInformation(memory, heap, bss, python, libpython)
