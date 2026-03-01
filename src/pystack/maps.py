"""Memory map data classes for process analysis.

This module provides data classes for representing memory maps.
The actual parsing is done in C++.
"""
import dataclasses
from pathlib import Path
from typing import Optional


@dataclasses.dataclass(frozen=True, eq=True)
class VirtualMap:
    """Represents a memory-mapped region in a process's virtual address space."""

    start: int
    end: int
    filesize: int
    offset: int
    device: str
    flags: str
    inode: int
    path: Optional[Path]

    def contains(self, addr: int) -> bool:
        """Check if the given address is within this memory map."""
        return self.start <= addr < self.end

    def is_executable(self) -> bool:
        """Check if this memory region is executable."""
        return "x" in self.flags

    def is_readable(self) -> bool:
        """Check if this memory region is readable."""
        return "r" in self.flags

    def is_writable(self) -> bool:
        """Check if this memory region is writable."""
        return "w" in self.flags

    def is_private(self) -> bool:
        """Check if this memory region is private (copy-on-write)."""
        return "p" in self.flags

    @property
    def size(self) -> int:
        """Return the size of this memory region."""
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
    """Represents a range of memory addresses."""

    min_addr: int
    max_addr: int


@dataclasses.dataclass
class MemoryMapInformation:
    """Container for memory map information needed for process analysis."""

    memory: MemoryRange
    heap: Optional[VirtualMap]
    bss: Optional[VirtualMap]
    python: VirtualMap
    libpython: Optional[VirtualMap]
