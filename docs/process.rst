.. _analyzing-processes:

Analyzing processes
*******************

The ``remote`` command is used to analyze the status of a remote process. The analysis is always done in a safe and
non-intrusive way as no code is loaded in the memory space of the process under analysis and no memory is modified
in the remote process. This makes the analysis using ``pystack`` a great option even for services and application
running in environments where the running process must not be impacted in any way. There are several options available:

.. argparse::
   :ref: pystack.__main__.generate_cli_parser
   :path: remote
   :prog: pystack

To use ``pystack`` you just need to provide the PID of the process: ::

    $ pystack remote 112
    Traceback for thread 112 [] (most recent call last):
        (Python) File "/test.py", line 17, in <module>
            first_func()
        (Python) File "/test.py", line 6, in first_func
            second_func()
        (Python) File "/test.py", line 10, in second_func
            third_func()
        (Python) File "/test.py", line 14, in third_func
            time.sleep(1000)

To learn more about the different options you can use to customize the report, check the :ref:`customizing-the-reports` section.

Operation modes
===============

.. note:: By default the **blocking** mode is used to guarantee correctness over speed.

There are two main operation modes when analyzing the status of a remote process:

* The **blocking** mode (default) will stop the process, reads its memory to produce the report and then continue
  its execution. The overhead of this operations is very small (normally is under 10ms) but it can be
  undesired in some scenarios such as services and application running in production environments. The
  advantage of this mode is that the memory retrieved is fully coherent which guarantees that the resulting
  report will always be correct.

* The **non blocking** mode (activated using ``--no-block``) will analyze the process memory to produce a
  report as the program is running. Although the memory retrieval normally is orders of magnitude faster than
  the rate at which the interpreter switches state, is still possible that the resulting report is not correct
  or that it cannot be produced due to incoherent results.

In general, users should prefer **blocking** mode (the default) because of its correctness unless stopping
momentarily the process is not acceptable, in which case **non blocking** mode can be used.

Interpreter finalization
========================

By default, ``pystack`` will try to only fetch the Python stack trace but there are some scenarios where there
will be no active Python stack trace even if the process is running. The most typical scenario is that the process
is shutting down and the Python interpreter is in the process of being deallocated. In this case you will see the
following error message with a warning explaining the situation: ::

    $ pystack remote 140
    WARNING(process_remote): The interpreter is shutting itself down so it is possible that no Python stack trace is
    available for inspection. You can still use --native-all  to force displaying all the threads.

    💀 Could not gather enough information to extract the Python frame information 💀

    ...

As the warning indicates, you can use `--native-all` to force displaying all the threads, even if there is no active
Python stack trace: ::

    $pystack remote 140 --native-all
    Traceback for thread 140 [Garbage collecting] (most recent call last):
        (C) File "???", line 0, in _start ()
        (C) File "???", line 0, in __libc_start_main ()
        (C) File "/opt/bb/src/python/python3.8/Modules/main.c", line 743, in Py_BytesMain (/opt/bb/lib64/libpython3.8.so.1.0)
        (C) File "/opt/bb/src/python/python3.8/Modules/main.c", line 691, in Py_RunMain (/opt/bb/lib64/libpython3.8.so.1.0)
        (C) File "/opt/bb/src/python/python3.8/Python/pylifecycle.c", line 1219, in Py_FinalizeEx (/opt/bb/lib64/libpython3.8.so.1.0)
        (C) File "/opt/bb/src/python/python3.8/Modules/gcmodule.c", line 1844, in PyGC_Collect (/opt/bb/lib64/libpython3.8.so.1.0)
        (C) File "/opt/bb/src/python/python3.8/Modules/gcmodule.c", line 1240, in collect_with_callback.constprop.33 (/opt/bb/lib64/libpython3.8.so.1.0)
        (C) File "/opt/bb/src/python/python3.8/Modules/gcmodule.c", line 1063, in collect.constprop.34 (/opt/bb/lib64/libpython3.8.so.1.0)
        (C) File "/opt/bb/src/python/python3.8/Modules/gcmodule.c", line 495, in move_unreachable (inlined) (/opt/bb/lib64/libpython3.8.so.1.0)

Here you can see that the process seems to be stucked while garbage collecting some object at finalization.

.. tip:: In some cases, is still possible to retrieve the Python stack trace by using the ``--exhaustive`` command line option.
