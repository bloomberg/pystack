.. _analyzing-core-dumps:

Analyzing core dumps
********************

The ``core`` subcommand is used to analyze the status of a core dump file. Analyzing core files is
very similar to analyzing processes but there are some differences, as the core file does not
contain the totality of the memory that was valid when the program was live. In most cases, this
makes no difference, as PyStack will try to adapt automatically. However, in some cases, you will need to
specify extra command line options to help PyStack locate the information it needs. When analyzing
cores, there are several options available:

.. argparse::
   :ref: pystack.__main__.generate_cli_parser
   :path: core
   :prog: pystack

In most cases, you just need to provide the location of the core to use PyStack with core dump files::

    $ pystack core ./the_core_file
    Using executable found in the core file: /usr/bin/python3.8

    Core file information:
    state: t zombie: True niceness: 0
    pid: 570 ppid: 1 sid: 1
    uid: 0 gid: 0 pgrp: 570
    executable: python3.8 arguments: python3.8

    The process died due receiving signal SIGSTOP
    Traceback for thread 570 [] (most recent call last):
        (Python) File "/test.py", line 19, in <module>
            first_func({1: None}, [1,2,3])
        (Python) File "/test.py", line 7, in first_func
            second_func(x, y)
        (Python) File "/test.py", line 12, in second_func
            third_func(x, y)
        (Python) File "/test.py", line 16, in third_func
            time.sleep(1000)

Pystack can automatically extract core dumps from `.gz` files: `pystack core ./archived_core_file.gz`

To learn more about the different options you can use to customize what is reported, check the :ref:`customizing-the-reports` section.

Providing the executable
========================

As the core file doesn't contain all the memory that was available in the
original process, there is some information that needs to be obtained from the
original Python executable that was running. If you just provide the location
of the core file on the command line, PyStack will try to automatically
locate the executable for you::

    $ pystack core ./the_core_file
    Using executable found in the core file: /usr/bin/python3.8

If the location of the executable is known, it can also by provided manually by
passing it as the second argument::

    $ pystack core ./the_core_file /usr/bin/python3.8

In most cases, providing just the location of the core file will work and
PyStack will be able to do a successful analysis, but is possible that the
executable being identified automatically is not the correct one. This can
happen if the process was launched through a script with a shebang or using
some form of indirection. For instance::

    $ pystack core ./pytest_core
    Using executable found in the core file: /usr/bin/pytest

    Core file information:
    state: t zombie: True niceness: 0
    pid: 770 ppid: 1 sid: 1
    uid: 0 gid: 0 pgrp: 770
    executable: python3.8 arguments: /usr/bin/python3.8

    The core file seems to have been generated on demand (the process did not crash)
    💀 Unable to find maps for the executable /usr/bin/pytest. These are the available executable memory maps: [dso], /usr/bin/python3.8 💀

This happens because ``/usr/bin/pytest`` is actually a shell script that
will be executed using a shebang and not the actual Python executable (located
in ``/usr/bin/python3.8``)::

    $ file /usr/bin/pytest
    /usr/bin/pytest: Python script, ASCII text executable

In this case, PyStack complains that it cannot match the memory maps in the
core file to the the provided executable and is providing the list of memory
maps that are available in the core::

    💀 Unable to find maps for the executable /usr/bin/pytest. These are the available executable memory maps: [dso], /usr/bin/python3.8 💀

.. note::
    PyStack matches the memory maps of the executable (whether provided manually
    or detected automatically) by the base name of the file. This means
    that if the there is a memory map for ``/usr/bin/python3.8`` and the
    provided (or automatically detected) executable is
    ``/usr/other_location/python3.8`` then ``pystack`` will match them
    because the base name is the same (``python3.8``) but if the provided
    executable is ``/usr/bin/python3.7`` then ``pystack`` will not match
    them because ``python3.7 != python3.8``.

To solve this, you must provide the path to the Python binary that was
used to execute the original process. We can use the list of available maps to
identify possible candidates for the executable, as it must match one of the
available memory maps that ``pystack`` includes in the error message. In
addition to the list of the memory maps, notice that ``pystack`` also includes
some useful information that greatly helps identifying the correct executable::

    Core file information:
    state: t zombie: True niceness: 0
    pid: 770 ppid: 1 sid: 1
    uid: 0 gid: 0 pgrp: 770
    executable: python3.8 arguments: /usr/bin/python3.8

Here we can easily see what executable was used (``python3.8``) and how it was
invoked. In this particular example is easy to also use the provided memory
maps because the Python executable is the only map that is a file among the
ones provided by ``pystack``: ``/usr/bin/python3.8``. To indicate to
``pystack`` that this is the executable we just need to pass it as the second
argument of the command line invocation::

    $ pystack core ./pytest_core /usr/bin/python3.8

    Core file information:
    state: t zombie: True niceness: 0
    pid: 770 ppid: 1 sid: 1
    uid: 0 gid: 0 pgrp: 770
    executable: python3.8 arguments: /usr/bin/python3.8

    The core file seems to have been generated on demand (the process did not crash)
    Traceback for thread 770 [Has the GIL] (most recent call last):
        (Python) File "/usr/bin/pytest", line 8, in <module>
            sys.exit(console_main())
        (Python) File "/usr/lib/python3.8/site-packages/_pytest/config/__init__.py", line 185, in console_main
            code = main()
        (Python) File "/usr/lib/python3.8/site-packages/_pytest/config/__init__.py", line 162, in main
            ret: Union[ExitCode, int] = config.hook.pytest_cmdline_main(
        ...


Core file information
=====================

To help users learn details of the program that generated the core file,
PyStack will try to display additional information based on various kernel
data structures at the time of the crash::

    Core file information:
    state: t zombie: True niceness: 0
    pid: 770 ppid: 1 sid: 1
    uid: 0 gid: 0 pgrp: 770
    executable: python3.8 arguments: /usr/bin/python3.8 -c "print('Hello')"

.. warning::
    The information provided in the "Core file information" section has a
    maximum size of 80 characters so it can be incomplete in some cases where
    large paths or command line arguments are involved.

This information displayed can include the following attributes:

- state: A letter describing the state of a given process. It can be one of:

  * ``d`` — uninterruptible sleep (usually IO):
  * ``i`` — idle kernel thread
  * ``r`` — running or runnable (on run queue)
  * ``s`` — interruptible sleep (waiting for an event to complete)
  * ``t`` — stopped by job control signal
  * ``t`` — stopped by debugger during the tracing
  * ``w`` — paging (not valid since the 2.6.xx kernel)
  * ``x`` — dead (should never be seen)
  * ``z`` — defunct ("zombie") process, terminated but not reaped by its parent

- zombie: A boolean described if the process was in a zombie state (terminated but not reaped by its parent)
  at the time of the crash.
- niceness: The value of the "niceness" parameter that represents the CPU priority in the scheduler.
- pid: The PID of the process.
- ppid: The PID of the parent of the process.
- sid: The Session ID of the process.
- uid: The User ID of the process.
- gid: The Group ID of the process.
- pgrp: The process group.
- executable: The base name of the executable that was used to start the process.
- arguments: The command line invocation of the process, truncated at 80 characters.

Origin of the core file
=======================

Some times core file are not generated because the program crashed, and this
can be a source of confusion as analyzing the stack trace report looking for
crashes will be a futile task because the process could have been in any
arbitrary healthy state. Even if the process has indeed crashed, knowing if it
was planned or not can be very important to perform a successful analysis. For
this reason, ``pystack`` will try to display why the core was generated.
Depending on the kernel version and the available information in the core file,
it can show several cases:

* The core file was generated on demand::

    The core file seems to have been generated on demand (the process did not crash)

* The core file was generated because some user sent a killing signal::

    The process died due receiving signal SIGBUS sent by pid 23

* The core file was generated because it received a segmentation fault signal::

    The process died due a segmentation fault accessing address: 0x75bcd15

Using this information, is possible to make sense of the displayed stack traces
when hunting for a specific problem.

Analyzing core files in a different machine
===========================================

In general, analyzing core files in the same machine where they were generated
will almost always yield a successful analysis using the default command line
options but analyzing core files in a different machine can lead to
complications. The main complication is that the shared libraries that were
mapped into the process and that are referred by the core are either not
available on the machine where the analysis is being performed or are different
(have a different build ID) than the ones that were mapped into the process
that generated the core. In this case, when analyzing the core file you will
see the following warnings::

    $ pystack core ./the_core /path/to/the_executable
    ...
    WARNING(process_core): Failed to locate /tmp/bundle/python/lib/python3.8/lib-dynload/_heapq.cpython-38-x86_64-linux-gnu.so
    WARNING(process_core): Failed to locate /tmp/bundle/binary-dependencies/libfreebl3.so
    WARNING(process_core): Failed to locate /tmp/bundle/binary-dependencies/libcrypt.so.1
    WARNING(process_core): Failed to locate /tmp/bundle/binary-dependencies/libpython3.8.so.1.0
    ...

.. caution::
    Note that is possible that ``pystack`` can still generate a valid report
    even if it fails to find some of the shared libraries that were linked into
    the process, so the presence of these warnings is not an error condition.
    Failing to provide shared libraries involved in the stack trace can make
    the ``--native`` and ``--native-all`` options not work properly.

This indicates that PyStack has failed to locate some shared libraries that
were loaded into the process. Loaded shared libraries' paths are recorded in
the core file, but if the core file is moved to a different machine, or if
libraries on the machine are updated or removed, then PyStack won't be able to
find the versions of the libraries that it needs to understand the core file.

Fortunately, if you have access to the shared libraries that were originally
used, it is possible to point PyStack to a new location for these shared
libraries. There are two options for pointing PyStack at libraries in different
locations than what was recorded in the core file:

* ``--lib-search-path`` accepts a colon-separated list of directories to search
  for libraries in (like the format of the ``PATH`` environment variable).
  For example, if you've copied a PyInstaller application to a new directory,
  you might use::

    $ pystack core ./the_core ./the_app/appname/appname --lib-search-path="./the_app/appname:./the_app/appname/lib-dynload"
    Traceback for thread 1340 [] (most recent call last):
    (Python) File "/src/tests/integration/single_thread_program.py", line 20, in <module>
        first_func()
    (Python) File "/src/tests/integration/single_thread_program.py", line 6, in first_func
        second_func()
    (Python) File "/src/tests/integration/single_thread_program.py", line 10, in second_func
        third_func()
    (Python) File "/src/tests/integration/single_thread_program.py", line 17, in third_func
        time.sleep(1000)

.. note::
   Directories are searched in the order they occur in the list, and libraries
   are matched by file name. If two directories in the search path both contain
   a file with the same name as a required shared library, the library from the
   directory that is listed earlier in the search path will be used.

* ``--lib-search-root`` allows providing a directory to be searched recursively
  for shared libraries. PyStack will automatically construct a search path
  containing every directory with shared libraries under that search root, then
  use that constructed path as though it was given as ``--lib-search-path``.
  For example::

    $ pystack core ./the_core ./the_app/appname/appname --lib-search-root=./the_app
    Traceback for thread 1340 [] (most recent call last):
    (Python) File "/src/tests/integration/single_thread_program.py", line 20, in <module>
        first_func()
    (Python) File "/src/tests/integration/single_thread_program.py", line 6, in first_func
        second_func()
    (Python) File "/src/tests/integration/single_thread_program.py", line 10, in second_func
        third_func()
    (Python) File "/src/tests/integration/single_thread_program.py", line 17, in third_func
        time.sleep(1000)

   The ``--lib-search-root`` option is especially useful for analyzing core
   files generated by self contained applications, such as those produced by
   PyInstaller's ``--onedir`` mode). When an application is bundled together
   with its binary dependencies, you only need to provide PyStack with the root
   folder and it will automatically find all of the bundled shared libraries.

.. warning::
   The ``--lib-search-root`` option adds directories to the library search path
   in lexicographic order. If a file with the same name as a shared library
   PyStack needs to load is found in two different directories under the search
   root, the wrong one may be used. When the order in which directories under
   the root are searched matters, use ``--lib-search-path`` instead.

.. tip::
   You can see the library search path that ``--lib-search-root`` has
   constructed when running in ``-v`` verbose mode.

Analyzing core files with insufficient information
==================================================

In some rare scenarios is possible that PyStack
won't have enough information to show the Python stack trace nor the native
stack trace (missing symbols, missing information in the executable, corrupt
core file information, etc). In these cases, is still possible to obtain the
Python stack trace of the core file by using the ``--exhaustive`` option. This
will trigger a more complete search for the interpreter information by
analyzing raw memory but will normally be slower and it will take a time
proportional to the core file size, as all the memory needs to be analysed to
do that.

.. tip::
    When using ``--exhaustive`` make sure you have the core file in a fast
    file-system (not NFS or docker mount points) to speed up the analysis.
