Deadlock
========

Intro
-----

This lesson is meant to familiarize you with PyStack with a classical
problem of locks acquisition.


In this exercise, we will intentionally create a lock ordering issue,
which is a common way of causing a deadlock, where two or more threads
are waiting for each other to release resources, causing the program to
hang indefinitely.

Development Environment Setup
-----------------------------

Navigate to the `PyStack GitHub repo <https://github.com/bloomberg/pystack>`_ and get a copy of the
source code. You can either clone it, or just download the zip, whatever is your preference here.

You will also need a terminal with ``python3`` installed, preferably one of the latest versions.

Once you have the repo ready to navigate, ``cd`` into the ``docs/tutorials`` folder:

.. code:: shell

    cd docs/tutorials/

It is here where we will be running the tests and exercises for the remainder of the tutorial.
For reference here are the official `python3 venv docs <https://docs.python.org/3/library/venv.html>`_.
You can also just follow along with the commands below.

Let's go ahead and setup our virtual environment.

.. code:: shell

    python3 -m venv .venv

Once your virtual environment has been created, you can activate it like so:

.. code:: shell

    source .venv/bin/activate

You can confirm activation was successful if your terminal prompt is prefixed with ``(.venv)``.
With our virtual environment ready, we can go ahead and install all the dependencies required
for the tutorial.

.. code:: shell

    python3 -m pip install -r requirements-tutorial.txt

Keep your virtual environment activated for the rest of the tutorial, and you should be able to run
any of the commands in the exercises.

PyStack remote use
------------------

PyStack remote has the ability to analyze the status of a running (remote) process.

Executing the deadlock
^^^^^^^^^^^^^^^^^^^^^^

In the ``docs/tutorials`` directory, there will be a script called ``deadlock.py``:

.. include:: deadlock.py
   :literal:

Assuming that we are in the ``docs/tutorials`` directory, we can run the deadlock script with:

.. code:: shell

    python3 deadlock.py

This script will intentionally perform a deadlock and the first message will contain the process ID, so this is the expected output:

.. code:: shell

    Process ID: <PID>
    Stating First Thread
        First lock acquired
    Stating Second Thread
        First lock acquired

Another option is to find it with:

.. code:: shell

    ps aux | grep deadlock.py

After the deadlock occurs we can use the PyStack command to analyze the
process (replace ``<PID>`` with the process ID from the previous step):

.. code:: shell

    pystack remote <PID>


If you see ``Operation not permitted``, you may want to instead run it with:

.. code:: shell

    sudo -E pystack remote <PID>


Understanding the results
^^^^^^^^^^^^^^^^^^^^^^^^^

The excepted result is the following code:

.. code:: shell

    Traceback for thread 789 (python3) [] (most recent call last):
        (Python) File "/<python_stdlib_path>/threading.py", line 966, in _bootstrap
            self._bootstrap_inner()
        (Python) File "/<python_stdlib_path>/threading.py", line 1009, in _bootstrap_inner
            self.run()
        (Python) File "/<python_stdlib_path>/threading.py", line 946, in run
            self._target(*self._args, **self._kwargs)
        (Python) File "/<path_to_tutorials>/deadlock.py", line 10, in background
            with lock2:

    Traceback for thread 456 (python3) [] (most recent call last):
        (Python) File "/<python_stdlib_path>/threading.py", line 966, in _bootstrap
            self._bootstrap_inner()
        (Python) File "/<python_stdlib_path>/threading.py", line 1009, in _bootstrap_inner
            self.run()
        (Python) File "/<python_stdlib_path>/threading.py", line 946, in run
            self._target(*self._args, **self._kwargs)
        (Python) File "/<path_to_tutorials>/deadlock.py", line 10, in background
            with lock2:

    Traceback for thread 123 (python3) [] (most recent call last):
        (Python) File "/<path_to_tutorials>/deadlock.py", line 28, in <module>
            t1.join()
        (Python) File "/<python_stdlib_path>/threading.py", line 1089, in join
            self._wait_for_tstate_lock()
        (Python) File "/<python_stdlib_path>/threading.py", line 1109, in _wait_for_tstate_lock
            if lock.acquire(block, timeout):

Notice that each section is displaying a running thread, and there are three threads here:

1. Thread 123 is the original thread that creates the other two, and waits for them
2. Thread 456 is ``t1``
3. Thread 789 is ``t2``

Each thread has a stack trace:

- The thread 789 is trying to acquire lock1 but is blocked because lock1 is already held by thread 456.
- The thread 456 is trying to acquire lock2 but is blocked because lock2 is already held by thread 789.
- The main thread 123 is calling ``join()`` on t1, waiting for it to finish. However, t1 cannot finish because it is stuck waiting to acquire a lock, leading to a deadlock.

This way we can successful diagnose that there are a improper ordering of lock acquisition.

Exploring more features
^^^^^^^^^^^^^^^^^^^^^^^

PyStack has some features that can help the process of diagnose the problem.
Using ``--locals`` you can obtain a simple string representation of the local variables
in the different frames as well as the function arguments.

When you run:

.. code:: shell

    pystack remote <PID> --locals

The excepted result is:

.. code:: shell

    Traceback for thread 789 (python3) [] (most recent call last):
        (Python) File "/<python_stdlib_path>/threading.py", line 966, in _bootstrap
            self._bootstrap_inner()
        Arguments:
            self: <Thread at 0x7f0c04b96080>
        (Python) File "/<python_stdlib_path>/threading.py", line 1009, in _bootstrap_inner
            self.run()
        Arguments:
            self: <Thread at 0x7f0c04b96080>
        (Python) File "/<python_stdlib_path>/threading.py", line 946, in run
            self._target(*self._args, **self._kwargs)
        Arguments:
            self: <Thread at 0x7f0c04b96080>
        (Python) File "/<path_to_tutorials>/deadlock.py", line 10, in background
            with lock2:
        Arguments:
            lock2: <_thread.lock at 0x7f0c04b90900>
            lock1: <_thread.lock at 0x7f0c04b90b40>

    Traceback for thread 456 (python3) [] (most recent call last):
        (Python) File "/<python_stdlib_path>/threading.py", line 966, in _bootstrap
            self._bootstrap_inner()
        Arguments:
            self: <Thread at 0x7f0c04b5bfd0>
        (Python) File "/<python_stdlib_path>/threading.py", line 1009, in _bootstrap_inner
            self.run()
        Arguments:
            self: <Thread at 0x7f0c04b5bfd0>
        (Python) File "/<python_stdlib_path>/threading.py", line 946, in run
            self._target(*self._args, **self._kwargs)
        Arguments:
            self: <Thread at 0x7f0c04b5bfd0>
        (Python) File "/<path_to_tutorials>/deadlock.py", line 10, in background
            with lock2:
        Arguments:
            lock2: <_thread.lock at 0x7f0c04b90b40>
            lock1: <_thread.lock at 0x7f0c04b90900>

    Traceback for thread 123 (python3) [] (most recent call last):
        (Python) File "/<path_to_tutorials>/deadlock.py", line 28, in <module>
            t1.join()
        (Python) File "/<python_stdlib_path>/threading.py", line 1089, in join
            self._wait_for_tstate_lock()
        Arguments:
            timeout: None
            self: <Thread at 0x7f0c04b5bfd0>
        (Python) File "/<python_stdlib_path>/threading.py", line 1109, in _wait_for_tstate_lock
            if lock.acquire(block, timeout):
        Arguments:
            timeout: -1
            block: True
            self: <Thread at 0x7f0c04b5bfd0>
        Locals:
            lock: <_thread.lock at 0x7f0c04bae100>

Observe that we have the same format of result, with one section for each thread.
However now, there are more information, the local variables and function arguments.

- In thread 789 and 456 we can identify the ID of each lock.
- The main thread 123 we can verify the Arguments of ``lock.acquire()``, that no timeout was set ``timeout: None`` and ``self`` refers to the thread object ``<Thread at 0x7f0c04b5bfd0>``.  Moreover, ``timeout`` is ``-1``, which usually an indefinite ``wait``, and ``block`` is ``True``, meaning it will block until the lock is acquired.
