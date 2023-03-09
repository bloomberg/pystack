import sys
import time


class A:
    def __init__(self):
        self.fifo = open(sys.argv[1], "w")

    def __del__(self, time=time):
        self.fifo.write("ready")
        self.fifo.close()
        time.sleep(1000)


x = A()
