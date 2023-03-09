import sys
import time


def first_func():
    second_func()


def second_func():
    third_func()


def third_func():
    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)


first_func()
