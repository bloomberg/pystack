PyStack
=======

PyStack is a tool that uses forbidden magic to let you inspect the stack frames of a running Python
process or a Python core dump, to learn easily and quickly what it is doing (or what it was doing
when it crashed) without having to interpret nasty CPython internals.

What PyStack can do
-------------------

PyStack has the following amazing features:

- 💻 Works with both running processes and core dump files.
- 🧵 Shows if each thread currently holds the Python GIL, is waiting to acquire it, or is
  currently dropping it.
- 🗑️ Shows if a thread is running a garbage collection cycle.
- 🐍 Optionally shows native function calls, as well as Python ones. In this mode, PyStack prints
  the native stack trace (C/C++/Rust function calls), except that the calls to Python callables are
  replaced with frames showing the Python code being executed, instead of showing the internal C
  code the interpreter used to make the call.
- 🔍 Automatically demangles symbols shown in the native stack.
- 📈 Includes calls to inlined functions in the native stack whenever enough debug information is
  available.
- 🔍 Optionally shows the values of local variables and function arguments in Python stack frames.
- 🔒 Safe to use on running processes. PyStack does not modify any memory or execute any code in
  a process that is running. It simply attaches just long enough to read some of the process's memory.
- ⚡ Optionally, it can perform a Python stack analysis without pausing the process at all. This
  minimizes impact to the debugged process, at the cost of potentially failing due to data races.
- 🚀 Super fast! It can analyze core files 10x faster than general-purpose tools like GDB.
- 🎯 Even works with aggressively optimized Python interpreter binaries.
- 🔍 Even works with Python interpreters' binaries that do not have symbols or debug information
  (Python stack only).
- 💥 Tolerates memory corruption well. Even if the process crashed due to memory corruption, PyStack
  can usually reconstruct the stack.
- 💼 Self-contained: it does not depend on external tools or programs other than the Python
  interpreter used to run PyStack itself.

Contents
--------

.. toctree::

   process
   corefile
   customizing_the_reports

.. toctree::
   :hidden:
   :caption: Project Information

.. toctree::
   :hidden:
   :caption: Hands-on Tutorial

   tutorials/deadlock
   tutorials/random_prime_number
   tutorials/core_tutorial

.. toctree::
  :caption: Project Information

  changelog


Indices and tables
------------------

* :ref:`genindex`
* :ref:`search`
