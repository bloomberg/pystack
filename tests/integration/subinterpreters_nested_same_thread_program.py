import sys
import threading
import time

from subinterpreters_shim import run_in_new_interpreter

fifo = sys.argv[1]

inner_code = f"""
import time

with open({fifo!r}, "w") as f:
    f.write("ready")

while True:
    time.sleep(1)
"""

outer_code = f"""
import sys
sys.path.insert(0, {sys.path[0]!r})

from subinterpreters_shim import run_in_new_interpreter
run_in_new_interpreter({inner_code!r})
"""

t = threading.Thread(target=run_in_new_interpreter, args=(outer_code,))
t.start()

while True:
    time.sleep(1)
