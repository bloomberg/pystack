SIGABRT Example
===============================

Intro
-----

This lesson is meant to familiarize you with the ``core`` subcommand to analyze the status of a core dump file. 
The steps will help you configure core dump outputs in your Linux environment, invoke a segmentation fault signal
from an example script, ``assert.py``, and then analyze the core dump file with the subcommand.


Configure Core Dump Parameters
""""""""""""""""""""""""""""""

First, we need to configure the size of the core dumps in our Linux environment. We will set an unlimited size to preserve all information 
for this tutorial. This can be done by running the ``ulimit`` command in your terminal

.. code:: shell

    # unlimited core size
    ulimit -c unlimited

    # if you want to limit size to 100 MB
    ulimit -c 100000

If you want to make this change permanent, you can add the following line to your ``~/.bashrc``, ``~/.bash_profile``, or ``~/.zshrc`` files
depending on your terminal preference:

.. code:: shell

    echo "ulimit -c unlimited" >> ~/.bashrc
    source ~/.bashrc


Next, we want to set core dump parameters. To specify where core dumps are saved and their naming conventions, 
you can configure the ``/proc/sys/kernel/core_pattern`` file.

.. code:: shell

    echo "/tmp/core-%e.%p.%h.%t" | sudo tee /proc/sys/kernel/core_pattern

In this example, we configured the file pattern to include the executable name (%e), process ID (%p), hostname (%h), 
and timestamp (%t). Then we save the file to the /tmp directory. If you would like to learn more about core dumps, Red Hat 
has more information `here <https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/7/html/developer_guide/debugging-crashed-application#debugging-crashed-application_understanding-core-dumps6>`_.

Generating the Core Dump
""""""""""""""""""""""""

Now, we can run ``assert.py`` which attempts to use a series of asserts to check assumptions of a variable. The script eventually will 
use stdlib to abort a function after failing an assert and cause an abnormal program termination. This does not execute normal cleanup functions. 
This actually terminates the process by raising a SIGABRT and invokes a core dump. More information abort from GNU can be found `here <https://https://www.gnu.org/software/libc/manual/html_node/Aborting-a-Program.html>`_.

.. code:: shell
    
    python3 assert.py

You should receive the following output from your terminal:

.. code:: shell

    Aborted (core dumped)

The core dump file will be located in the directory specified by your core_pattern, which is ``/tmp`` in this case. 
You can list the core dump files in ``/tmp`` with the following command:

.. code:: shell

    # based on the pattern we set, the core dump file should be named core-<executable_name>.<process_id>.<hostname>.<timestamp>
    ls /tmp/core-*


Analyze the Core Dump
"""""""""""""""""""""

Now that we have the core dump file, we can use the ``core`` subcommand to analyze it like such:

.. code:: shell

    pystack core /tmp/core-<executable_name>.<process_id>.<hostname>.<timestamp> 

The output will display the stack trace of the core dump file, which will help you identify the source of the error.

.. code:: shell

    Using executable found in the core file: /src/.venv/bin/python

    Core file information:
    state: R zombie: True niceness: 0
    pid: 90266 ppid: 14133 sid: 14133
    uid: 0 gid: 0 pgrp: 90266
    executable: python arguments: python docs/tutorials/assert.py 

    The process died due receiving signal SIGABRT
    Traceback for thread 90266 [] (most recent call last):
        (Python) File "/src/docs/tutorials/assert.py", line 28, in <module>
            assertion()
        (Python) File "/src/docs/tutorials/assert.py", line 21, in assertion
            libc.abort()


You can also use the ``--native`` flag to display the native (C) frames in the resulting stack trace.

.. code:: shell

    pystack core --native /tmp/core-<executable_name>.<process_id>.<hostname>.<timestamp>

This output will display the native output of the core dump file, which will help you identify the source of the abort.

.. code:: shell 

    Using executable found in the core file: /src/.venv/bin/python

    Core file information:
    state: R zombie: True niceness: 0
    pid: 90266 ppid: 14133 sid: 14133
    uid: 0 gid: 0 pgrp: 90266
    executable: python arguments: python docs/tutorials/assert.py 

    The process died due receiving signal SIGABRT
    Traceback for thread 90266 [] (most recent call last):
        (C) File "../Objects/iterobject.c", line 311, in _start (python)
        (C) File "../csu/libc-start.c", line 392, in __libc_start_main@@GLIBC_2.34 (libc.so.6)
        (C) File "../sysdeps/nptl/libc_start_call_main.h", line 58, in __libc_start_call_main (libc.so.6)
        (C) File "../Modules/main.c", line 763, in Py_BytesMain (python)
        (C) File "../Modules/main.c", line 709, in Py_RunMain (python)
        (C) File "../Modules/main.c", line 629, in pymain_run_python (inlined) (python)
        (C) File "../Modules/main.c", line 379, in pymain_run_file (inlined) (python)
        (C) File "../Modules/main.c", line 360, in pymain_run_file_obj (inlined) (python)
        (C) File "../Python/pythonrun.c", line 1643, in pyrun_file.lto_priv.0 (python)
        (C) File "../Python/pythonrun.c", line 1743, in run_mod.lto_priv.0 (python)
        (C) File "../Python/pythonrun.c", line 1722, in run_eval_code_obj.lto_priv.0 (python)
        (Python) File "/src/docs/tutorials/assert.py", line 28, in <module>
            assertion()
        (Python) File "/src/docs/tutorials/assert.py", line 21, in assertion
            libc.abort()
        (C) File "../Modules/_ctypes/_ctypes.c", line 4167, in PyCFuncPtr_call.cold (_ctypes.cpython-312-x86_64-linux-gnu.so)
        (C) File "../Modules/_ctypes/callproc.c", line 1273, in _ctypes_callproc.cold (_ctypes.cpython-312-x86_64-linux-gnu.so)
        (C) File "../Modules/_ctypes/callproc.c", line 931, in _call_function_pointer (inlined) (_ctypes.cpython-312-x86_64-linux-gnu.so)
        (C) File "./stdlib/abort.c", line 79, in abort (libc.so.6)
        (C) File "../sysdeps/posix/raise.c", line 26, in raise (libc.so.6)
        (C) File "./nptl/pthread_kill.c", line 89, in pthread_kill@@GLIBC_2.34 (libc.so.6)
        (C) File "./nptl/pthread_kill.c", line 78, in __pthread_kill_internal (inlined) (libc.so.6)
        (C) File "./nptl/pthread_kill.c", line 44, in __pthread_kill_implementation (inlined) (libc.so.6)    

Conclusion
""""""""""

In this tutorial, we learned how to use the ``core`` subcommand to stack trace core dumps. In our example, we terminated a running process 
by raising a SIGABRT signal to invoke a core dump. We then analyzed the core dump file using the ``pystack`` command to identify the source of the 
fault. By analyzing the stack trace, we identified the source of the error. Using the `--native` flag, we viewed the raw output of the core dump file, 
which provided further insights into the cause of the error. By understanding how to analyze core dump files, we can effectively debug and 
troubleshoot the stack of our programs. Thank you for following along with this tutorial. Happy coding!
