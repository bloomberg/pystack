import sys
import threading
import time

threads_active = 0


def thread_func_1():
    thread_func_2()


def thread_func_2():
    thread_func_3()


def thread_func_3():
    global threads_active
    threads_active += 1
    time.sleep(1000)


def first_func():
    second_func()


def second_func():
    third_func()


def third_func():
    threads = [threading.Thread(target=thread_func_1) for _ in range(3)]
    for thread in threads:
        thread.start()
    while threads_active != 3:
        time.sleep(0.1)
    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)


first_func()
