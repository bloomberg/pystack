import os
import sys
import threading
import time

from subinterpreters_shim import run_in_new_interpreter

r_fd, w_fd = os.pipe()


def launch_chain():
    level3_code = f"import os, time; os.write({w_fd}, b'x'); time.sleep(86400)"
    level2_code = (
        f"import sys; sys.path.insert(0, {sys.path[0]!r}); "
        f"from subinterpreters_shim import run_in_new_interpreter as f;"
        f"f({level3_code!r})"
    )
    level1_code = (
        f"import sys; sys.path.insert(0, {sys.path[0]!r}); "
        f"from subinterpreters_shim import run_in_new_interpreter as f;"
        f"f({level2_code!r})"
    )
    run_in_new_interpreter(level1_code)


t1 = threading.Thread(target=launch_chain, daemon=True)
t2 = threading.Thread(target=launch_chain, daemon=True)
t1.start()
t2.start()

data = b""
while len(data) < 2:
    data += os.read(r_fd, 2 - len(data))
os.close(r_fd)
os.close(w_fd)

fifo = sys.argv[1]
with open(fifo, "w") as f:
    f.write("ready")

while True:
    time.sleep(1)
