import ctypes
import sys
import time


def first_func():
    second_func()


def second_func():
    third_func()


def third_func():
    # Trigger libffi to re-import the Python binary
    global gil_check
    gil_check = ctypes.CFUNCTYPE(ctypes.c_int)(ctypes.CDLL(None).PyGILState_Check)

    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)


first_func()
