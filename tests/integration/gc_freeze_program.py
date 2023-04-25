# type: ignore
import gc
import sys
import threading
import time


class A:
    def __del__(self):
        fifo = sys.argv[1]
        with open(sys.argv[1], "w") as fifo:
            fifo.write("ready")
        time.sleep(1000)


if __name__ == "__main__":
    t = threading.Thread(target=time.sleep, args=(1000,))
    t.start()
    a = A()
    x = []
    x.append(a)
    x.append(x)
    del a
    del x
    gc.collect()
