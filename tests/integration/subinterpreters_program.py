import os
import sys
import threading
import time

from subinterpreters_shim import run_in_new_interpreter

NUM_INTERPRETERS = 3

r_fd, w_fd = os.pipe()


def start_interpreter_async(code):
    t = threading.Thread(target=run_in_new_interpreter, args=(code,))
    t.start()


CODE = f"""
import os
import time

os.write({w_fd}, b"x")

while True:
    time.sleep(1)
"""

for _ in range(NUM_INTERPRETERS):
    start_interpreter_async(CODE)

data = b""
while len(data) < NUM_INTERPRETERS:
    data += os.read(r_fd, NUM_INTERPRETERS - len(data))
os.close(r_fd)
os.close(w_fd)

fifo = sys.argv[1]
with open(fifo, "w") as f:
    f.write("ready")

while True:
    time.sleep(1)
