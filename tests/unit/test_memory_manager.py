import ctypes
import mmap
import os

import pytest

from pystack._pystack import _copy_memory_for_testing


def test_reads_accessible_memory_from_map_with_guard_page(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv("_PYSTACK_NO_PROCESS_VM_READV", raising=False)
    page_size = mmap.PAGESIZE
    mapping = mmap.mmap(-1, page_size * 3)
    address = ctypes.addressof(ctypes.c_char.from_buffer(mapping))
    expected = b"pthread data"
    requested_address = address + page_size
    mapping[page_size : page_size + len(expected)] = expected

    libc = ctypes.CDLL(None, use_errno=True)
    mprotect = libc.mprotect
    mprotect.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]
    mprotect.restype = ctypes.c_int
    assert mprotect(address, page_size, 0) == 0
    try:
        result = _copy_memory_for_testing(
            os.getpid(),
            address,
            address + len(mapping),
            [(requested_address, len(expected)), (requested_address, len(expected))],
        )
    finally:
        assert mprotect(address, page_size, mmap.PROT_READ | mmap.PROT_WRITE) == 0
        mapping.close()

    assert result == [expected, expected]
