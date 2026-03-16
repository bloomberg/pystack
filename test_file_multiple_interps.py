import os
import threading
import time
from concurrent import interpreters

print(os.getpid())
NUM_INTERPRETERS = 3


def start_interpreter_async(interp, code):
    t = threading.Thread(target=interp.exec, args=(code,))
    t.daemon = True
    t.start()
    return t


CODE = """
import time
import threading
print(threading.get_native_id())
print(f"Hello from sub-interpreter")
while True:
    time.sleep(1)
"""

for i in range(NUM_INTERPRETERS):
    interp = interpreters.create()
    start_interpreter_async(interp, CODE)

print("Main interpreter sleeping forever...")
while True:
    time.sleep(1)
