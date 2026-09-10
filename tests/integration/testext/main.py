import sys

import testext


def first_func():
    second_func()


def second_func():
    third_func()


def third_func():
    # Blocks forever inside a C signal handler, after announcing we're ready.
    testext.signal_readiness_then_block(sys.argv[1])


first_func()
