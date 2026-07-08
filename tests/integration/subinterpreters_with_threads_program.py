import os
import sys
import threading
import time

from subinterpreters_shim import run_in_new_interpreter

NUM_INTERPRETERS = 2
NUM_THREADS_PER_SUBINTERPRETER = 2

r_fd, w_fd = os.pipe()


def start_interpreter_async(code):
    t = threading.Thread(target=run_in_new_interpreter, args=(code,))
    t.daemon = True
    t.start()
    return t


CODE = """\
import os
import threading
import time

NUM_THREADS = 2

def worker():
    os.write(%d, b"x")
    while True:
        time.sleep(1)

threads = []
for _ in range(NUM_THREADS):
    t = threading.Thread(target=worker)
    t.start()
    threads.append(t)

os.write(%d, b"x")
while True:
    time.sleep(1)
""" % (w_fd, w_fd)

threads = []
for _ in range(NUM_INTERPRETERS):
    t = start_interpreter_async(CODE)
    threads.append(t)

TOTAL_EXPECTED = NUM_INTERPRETERS * (NUM_THREADS_PER_SUBINTERPRETER + 1)

data = b""
while len(data) < TOTAL_EXPECTED:
    data += os.read(r_fd, TOTAL_EXPECTED - len(data))
os.close(r_fd)
os.close(w_fd)

fifo = sys.argv[1]
with open(fifo, "w") as f:
    f.write("ready")

while True:
    time.sleep(1)
