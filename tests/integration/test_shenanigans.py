import sys
from pathlib import Path

from pystack.engine import get_process_threads
from tests.utils import spawn_child_process

TEST_DUPLICATE_SYMBOLS_FILE = Path(__file__).parent / "ctypes_program.py"


def test_duplicate_pyruntime_symbol_handling(tmpdir):
    """Test that pystack correctly handles duplicate _PyRuntime symbols.

    This can occur when ctypes uses libffi to dlopen the Python binary
    in order to create a trampoline (which it only does if the Python binary
    was statically linked against libpython).
    """
    # GIVEN
    with spawn_child_process(
        sys.executable, TEST_DUPLICATE_SYMBOLS_FILE, tmpdir
    ) as child_process:
        # WHEN
        threads = list(get_process_threads(child_process.pid, stop_process=True))

    # THEN
    # We should have successfully resolved threads without "Invalid address" errors
    assert threads is not None
    assert len(threads) > 0

    # Verify we can get stack traces (which requires correct _PyRuntime)
    for thread in threads:
        # Just ensure we can get frames without crashing
        frames = list(thread.frames)
        # The main thread should have at least one frame
        if thread.tid == child_process.pid:
            assert len(frames) > 0
