Pystack
=======

Pystack is a tool that uses forbidden magic to allow you to inspect the stack frame of a remote Python process to know what it is doing.

What Pystack can do
-------------------

Pystack has the following amazing features:

-  Works with both live processes and core files.
-  It can tell you if a thread has the Python GIL, if is waiting for it
   or if is currently dropping it.
-  It can tell you if a given thread is garbage collecting.
-  Obtain the merged Python/native traceback to better debug and analyze
   extension modules and native code. This means that you will obtain
   the native stack trace (C/C++ function calls) but when the
   interpreter calls a Python function, the Python name, file and line
   number will be shown at that point instead of the internal C code
   that the interpreter uses to do such calls.
-  It can show inlined and overloaded native function calls.
-  It can show values of local variables and function arguments of
   Python stack frames.
-  Automatic demangling of symbols.
-  Is always safe to use in running processes: Pystack does not modify
   or execute any code in the running process at all: it just read some
   segments of its memory and the ELF files referenced by the memory
   maps.
-  It can perform a Python stack analysis without stopping the process
   at all.
-  Is super fast! It can analyze core files 10x faster than other
   general-purpose tools like gdb.
-  Works with aggressively optimized binaries like the ones we use at
   Bloomberg for the Python interpreters.
-  Works with binaries that do not have symbols or DWARF information
   (Python stack only).
-  Self-contained: it does not depend on external tools or programs
   other than the Python interpreter used to run Pystack itself.


Contents
--------

.. toctree::

   customizing_the_reports
   process
   corefile
   howitworks


Indices and tables
------------------

* :ref:`genindex`
* :ref:`search`
