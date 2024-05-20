import os
import threading
import time


def background(first_lock, second_lock):
    with first_lock:
        print("    First lock acquired")
        time.sleep(1)
        with second_lock:
            print("    Second lock acquired")


if __name__ == "__main__":
    print(f"Process ID: {os.getpid()}")
    lock_a = threading.Lock()
    lock_b = threading.Lock()

    t1 = threading.Thread(target=background, args=(lock_a, lock_b))
    t2 = threading.Thread(target=background, args=(lock_b, lock_a))

    print("Starting First Thread")
    t1.start()
    print("Starting Second Thread")
    t2.start()

    t1.join()
    t2.join()

    print("Finished execution")
