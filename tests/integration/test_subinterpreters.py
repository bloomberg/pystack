import io
from contextlib import redirect_stdout
from pathlib import Path

from pystack.engine import NativeReportingMode
from pystack.engine import get_process_threads
from pystack.traceback_formatter import TracebackPrinter
from tests.utils import ALL_PYTHONS_THAT_SUPPORT_SUBINTERPRETERS
from tests.utils import spawn_child_process

NUM_INTERPRETERS = 3

PROGRAM = f"""\
import sys
import threading
import time

from concurrent import interpreters

NUM_INTERPRETERS = {NUM_INTERPRETERS}


def start_interpreter_async(interp, code):
    t = threading.Thread(target=interp.exec, args=(code,))
    t.daemon = True
    t.start()
    return t


CODE = '''\\
import time
while True:
    time.sleep(1)
'''

threads = []
for i in range(NUM_INTERPRETERS):
    interp = interpreters.create()
    t = start_interpreter_async(interp, CODE)
    threads.append(t)

# Give sub-interpreters time to start executing
time.sleep(1)

fifo = sys.argv[1]
with open(fifo, "w") as f:
    f.write("ready")

while True:
    time.sleep(1)
"""


@ALL_PYTHONS_THAT_SUPPORT_SUBINTERPRETERS
def test_subinterpreters(python, tmpdir):
    """Test that pystack can detect and report multiple sub-interpreters."""

    # GIVEN
    _, python_executable = python
    test_file = Path(str(tmpdir)) / "subinterpreters_program.py"
    test_file.write_text(PROGRAM)

    # WHEN
    with spawn_child_process(python_executable, test_file, tmpdir) as child_process:
        threads = list(get_process_threads(child_process.pid, stop_process=True))

    # Collect all interpreter IDs from the threads
    interp_ids = {thread.interp_id for thread in threads}

    # THEN

    # We expect the main interpreter (0) plus NUM_INTERPRETERS sub-interpreters
    assert 0 in interp_ids
    assert len(interp_ids) == NUM_INTERPRETERS + 1

    # Verify the TracebackPrinter output contains the interpreter headers
    printer = TracebackPrinter(
        native_mode=NativeReportingMode.OFF,
        include_subinterpreters=True,
    )
    output = io.StringIO()
    with redirect_stdout(output):
        for thread in threads:
            printer.print_thread(thread)

    result = output.getvalue()
    assert "Interpreter-0 (main)" in result
    for interp_id in interp_ids:
        if interp_id == 0:
            continue
        assert f"Interpreter-{interp_id}" in result
