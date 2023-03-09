import sys


def first_func():
    second_func()


def second_func():
    third_func()


def third_func():
    collection = list(range(10000))
    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    while True:
        sorted(collection, reverse=True)


first_func()
