import functools
import sys
import time


def ham():
    spam()


def spam():
    functools.partial(foo)()


def foo():
    bar()


def bar():
    baz()


def baz():
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)


ham()
