.. _customizing-the-reports:

Customizing the reports
***********************

By default, PyStack will show you the Python stack trace for all threads registered with the interpreter, but this
might not be enough in some scenarios where more detailed information is needed. PyStack offers some customization options
to include more information in the report. These options are available whether analyzing live processes or core files.

Native traces
=============

.. highlight:: text

Sometimes, having only the Python stack trace is not enough to understand what's happening in the process,
especially when the interpreter is performing some internal operation or
when some C extension code is being executed. As in these cases the computations are being executed using
machine code, the Python stack trace will not show the most relevant information. Although it is possible to obtain native
stack traces using tools like ``pstack`` or ``gdb``, these traces will not show the Python frames that happen between
the different native calls, making it very difficult to interpret the reported stack trace.

In these situations, the best possible view is a "merged" stack trace in which the native calls that are used to execute
Python code are substituted by the actual Python functions being executed. This means that instead of::

        ...
        (C) File "Objects/call.c", line 411, in _PyFunction_Vectorcall (/usr/lib/libpython3.8.so.1.0)
        (C) File "Include/cpython/abstract.c", line 127, in _PyObject_Vectorcall (/usr/lib/libpython3.8.so.1.0)
        (C) File "Objects/ceval.c", line 123, in _PyEval_EvalFrameDefault (/usr/lib/libpython3.8.so.1.0)
        (C) File "Python/ceval.c", line 4963, in call_function (inlined) (/usr/lib/libpython3.8.so.1.0)
        (C) File "Modules/timemodule.c", line 338, in time_sleep (/usr/lib/libpython3.8.so.1.0)
        (C) File "Modules/timemodule.c", line 1866, in pysleep (inlined) (/usr/lib/libpython3.8.so.1.0)
        (C) File "???", line 0, in __select ()

you will see::

        ...
        (Python) File "/test.py", line 14, in third_func
            time.sleep(1000)
        (C) File "Python/ceval.c", line 4963, in call_function (inlined) (/usr/lib/libpython3.8.so.1.0)
        (C) File "Modules/timemodule.c", line 338, in time_sleep (/usr/lib/libpython3.8.so.1.0)
        (C) File "Modules/timemodule.c", line 1866, in pysleep (inlined) (/usr/lib/libpython3.8.so.1.0)
        (C) File "???", line 0, in __select ()

which shows how the call to ``time.sleep(1000)`` invokes the internal calls to the internal C implementation of the ``time`` module,
ending with the call to ``__select()`` in ``libc``. This shows you the full picture of what the full sequence of calls between Python
and native code is without losing any important details.

.. important::
   Native mode (actived using ``--native``) requires the interpreter and C extensions and shared libraries to have debugging
   symbols. If debugging symbols have been stripped it is possible that the displayed stack traces will be incomplete.

To get the merged native stack traces you can use the ``--native`` command line option::

    $ pystack remote 112 --native
    Traceback for thread 112 [] (most recent call last):
        (C) File "???", line 0, in _start ()
        (C) File "???", line 0, in __libc_start_main ()
        (C) File "Modules/main.c", line 743, in Py_BytesMain (/usr/lib/libpython3.8.so.1.0)
        (C) File "Modules/main.c", line 689, in Py_RunMain (/usr/lib/libpython3.8.so.1.0)
        (C) File "Modules/main.c", line 610, in pymain_run_python (inlined) (/usr/lib/libpython3.8.so.1.0)
        (C) File "Modules/main.c", line 385, in pymain_run_file (inlined) (/usr/lib/libpython3.8.so.1.0)
        (C) File "Python/pythonrun.c", line 472, in PyRun_SimpleFileExFlags (/usr/lib/libpython3.8.so.1.0)
        (C) File "Python/pythonrun.c", line 439, in pyrun_simple_file (inlined) (/usr/lib/libpython3.8.so.1.0)
        (C) File "Python/pythonrun.c", line 1085, in pyrun_file (/usr/lib/libpython3.8.so.1.0)
        (C) File "Python/pythonrun.c", line 1188, in run_mod (/usr/lib/libpython3.8.so.1.0)
        (C) File "Python/pythonrun.c", line 1166, in run_eval_code_obj (/usr/lib/libpython3.8.so.1.0)
        (Python) File "/test.py", line 17, in <module>
            first_func()
        (C) File "Python/ceval.c", line 4963, in call_function (inlined) (/usr/lib/libpython3.8.so.1.0)
        (C) File "Objects/call.c", line 284, in function_code_fastcall (inlined) (/usr/lib/libpython3.8.so.1.0)
        (Python) File "/test.py", line 6, in first_func
            second_func()
        (C) File "Python/ceval.c", line 4963, in call_function (inlined) (/usr/lib/libpython3.8.so.1.0)
        (C) File "Objects/call.c", line 284, in function_code_fastcall (inlined) (/usr/lib/libpython3.8.so.1.0)
        (Python) File "/test.py", line 10, in second_func
            third_func()
        (C) File "Python/ceval.c", line 4963, in call_function (inlined) (/usr/lib/libpython3.8.so.1.0)
        (C) File "Objects/call.c", line 284, in function_code_fastcall (inlined) (/usr/lib/libpython3.8.so.1.0)
        (Python) File "/test.py", line 14, in third_func
            time.sleep(1000)
        (C) File "Python/ceval.c", line 4963, in call_function (inlined) (/usr/lib/libpython3.8.so.1.0)
        (C) File "Modules/timemodule.c", line 338, in time_sleep (/usr/lib/libpython3.8.so.1.0)
        (C) File "Modules/timemodule.c", line 1866, in pysleep (inlined) (/usr/lib/libpython3.8.so.1.0)
        (C) File "???", line 0, in __select ()

Locals
======

In some scenarios knowing the stack trace is not enough to understand the internal state of the program. For instance, a given
function can do very different things depending on the provided arguments or different code paths can be taken depending on the
value of some local variable. In these cases, you can use ``--locals`` to obtain a simple string representation of the local variables
in the different frames as well as the function arguments::

    $ pystack remote 117 --locals
    Traceback for thread 117 [] (most recent call last):
        (Python) File "/test.py", line 19, in <module>
            first_func({1: None}, [1,2,3])
        (Python) File "/test.py", line 7, in first_func
            second_func(x, y)
          Arguments:
            y: [1, 2, 3]
            x: {1: None}
          Locals:
            some_variable: "hello from pystack"
        (Python) File "/test.py", line 12, in second_func
            third_func(x, y)
          Arguments:
            y: [1, 2, 3]
            x: {1: None}
          Locals:
            answer: 42
        (Python) File "/test.py", line 16, in third_func
            time.sleep(1000)
          Arguments:
            y: [1, 2, 3]
            x: {1: None}

Only certain types of local variables can be printed this way, but most of the common built-in types can.

.. warning::
    Using ``--locals`` can slightly increase the time required to generate a report due to the amount of extra memory
    that needs to be copied to display the local variables and function arguments. Is also advised not to use the ``--no-block``
    option in this case as the process may evolve too fast in the time ``pystack`` is fetching the local variables.

.. tip:: For the most complete view you can combine ``--locals`` with ``--native`` or ``--native--all``.
