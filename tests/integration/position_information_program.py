import sys
import time

# flake8: noqa
# fmt: off

class A:
    def __getitem__(self, item):
        return second_func(item) + second_func(item)


def first_func(x):
    aaaa = A()
    1 + 2 + aaaa[12] + 3


def second_func(x):
    short_arg = x
    loooooooooooooooong_arg = x
    third_func(short_arg, short_arg, 1 + loooooooooooooooong_arg
        , short_arg, short_arg, loooooooooooooooong_arg
    )


def third_func(*args):
    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)


first_func(1)

# fmt: on
