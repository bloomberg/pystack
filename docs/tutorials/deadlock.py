import os
import threading
import time


def background(lock1, lock2):
    with lock1:
        print("    First lock acquired")
        time.sleep(1)
        with lock2:
            print("    Second lock acquired")


if __name__ == "__main__":
    print(f"Process ID: {os.getpid()}")
    lock1 = threading.Lock()
    lock2 = threading.Lock()

    t1 = threading.Thread(target=background, args=(lock1, lock2))
    t2 = threading.Thread(target=background, args=(lock2, lock1))

    print("Stating First Thread")
    t1.start()
    print("Stating Second Thread")
    t2.start()

    t1.join()
    t2.join()

    print("Finished execution")
