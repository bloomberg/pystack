.. _multiple-interpreters:

Multiple interpreters
*********************

If your process uses multiple interpreters (for example, using the `concurrent.interpreters` module
or a `concurrent.futures.InterpreterPoolExecutor`), PyStack will show which interpreter each stack
is associated with, and will even show where a thread switches from one interpreter to another.

.. note::
   This feature only works for processes running Python 3.11 or newer.

Example program with multiple interpreters
==========================================

For instance, given this Python 3.14 program::

    import os
    import signal
    from concurrent.futures import InterpreterPoolExecutor


    def interpreter1_body(read_fd):
        print("Hello from interpreter 1!")
        open(read_fd, closefd=False).read()
        print("Goodbye from interpreter 1!")


    def interpreter2_body(read_fd):
        print("Greetings from interpreter 2!")
        open(read_fd, closefd=False).read()
        print("Farewell from interpreter 2!")


    read_fd, write_fd = os.pipe()

    with InterpreterPoolExecutor(max_workers=2) as executor:
        executor.submit(interpreter1_body, read_fd)
        executor.submit(interpreter2_body, read_fd)

        try:
            signal.pause()
        except KeyboardInterrupt:
            print("\nCtrl-C received, signaling workers to stop...")
            os.close(write_fd)
    os.close(read_fd)

If you run it until it pauses and then analyze it with PyStack, you will see output like this::

    $ pystack remote 392462
    Traceback for thread 392464 (InterpreterPool) (most recent call last):
      In the main interpreter []
        (Python) File "/usr/lib/python3.14/threading.py", line 1044, in _bootstrap
            self._bootstrap_inner()
        (Python) File "/usr/lib/python3.14/threading.py", line 1082, in _bootstrap_inner
            self._context.run(self.run)
        (Python) File "/usr/lib/python3.14/threading.py", line 1024, in run
            self._target(*self._args, **self._kwargs)
        (Python) File "/usr/lib/python3.14/concurrent/futures/thread.py", line 119, in _worker
            work_item.run(ctx)
        (Python) File "/usr/lib/python3.14/concurrent/futures/thread.py", line 86, in run
            result = ctx.run(self.task)
        (Python) File "/usr/lib/python3.14/concurrent/futures/interpreter.py", line 84, in run
            return self.interp.call(do_call, self.results, *task)
        (Python) File "/usr/lib/python3.14/concurrent/interpreters/__init__.py", line 238, in call
            return self._call(callable, args, kwargs)
        (Python) File "/usr/lib/python3.14/concurrent/interpreters/__init__.py", line 220, in _call
            res, excinfo = _interpreters.call(self._id, callable, args, kwargs, restrict=True)
      In interpreter 2 []
        (Python) File "/usr/lib/python3.14/concurrent/futures/interpreter.py", line 11, in do_call
            return func(*args, **kwargs)
        (Python) File "/tmp/interpreter_demo.py", line 14, in interpreter2_body
            open(read_fd, closefd=False).read()

    Traceback for thread 392463 (InterpreterPool) (most recent call last):
      In the main interpreter []
        (Python) File "/usr/lib/python3.14/threading.py", line 1044, in _bootstrap
            self._bootstrap_inner()
        (Python) File "/usr/lib/python3.14/threading.py", line 1082, in _bootstrap_inner
            self._context.run(self.run)
        (Python) File "/usr/lib/python3.14/threading.py", line 1024, in run
            self._target(*self._args, **self._kwargs)
        (Python) File "/usr/lib/python3.14/concurrent/futures/thread.py", line 119, in _worker
            work_item.run(ctx)
        (Python) File "/usr/lib/python3.14/concurrent/futures/thread.py", line 86, in run
            result = ctx.run(self.task)
        (Python) File "/usr/lib/python3.14/concurrent/futures/interpreter.py", line 84, in run
            return self.interp.call(do_call, self.results, *task)
        (Python) File "/usr/lib/python3.14/concurrent/interpreters/__init__.py", line 238, in call
            return self._call(callable, args, kwargs)
        (Python) File "/usr/lib/python3.14/concurrent/interpreters/__init__.py", line 220, in _call
            res, excinfo = _interpreters.call(self._id, callable, args, kwargs, restrict=True)
      In interpreter 1 []
        (Python) File "/usr/lib/python3.14/concurrent/futures/interpreter.py", line 11, in do_call
            return func(*args, **kwargs)
        (Python) File "/tmp/interpreter_demo.py", line 8, in interpreter1_body
            open(read_fd, closefd=False).read()

    Traceback for thread 392462 (python3.14) (most recent call last):
      In the main interpreter []
        (Python) File "/tmp/interpreter_demo.py", line 25, in <module>
            signal.pause()

Note that the output now shows which interpreter each thread is running in, and also shows the
transition from the main interpreter to the subinterpreters. The GIL status is shown separately for
each interpreter, as a thread may hold the GIL for one interpreter but not another. Likewise, the GC
status is shown separately, since a thread may be running a GC cycle in one interpreter but not
another.
