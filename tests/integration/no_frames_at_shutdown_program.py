import atexit
import functools
import sys
import time

atexit.register(functools.partial(time.sleep, 1000))

fifo = sys.argv[1]
with open(sys.argv[1], "w") as fifo:
    fifo.write("ready")
