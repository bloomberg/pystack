<img src="https://bbgithub.dev.bloomberg.com/storage/user/9113/files/94cbfe00-a4c9-11ea-9208-5b77a73547cd" align="right" height="150" width="150"/>

# Pystack

[![](https://badges.dev.bloomberg.com/badge/Status/Mature/green?icon=md-trending_flat)](http://tutti.prod.bloomberg.com/python-docs/src/release-states)
[![Build Status](https://python.jaas.dev.bloomberg.com/buildStatus/icon?job=python/pystack/main)](https://python.jaas.dev.bloomberg.com/job/python/job/pystack/job/main)
![python.8](https://badges.dev.bloomberg.com/badge/python/3.8/blue?icon=python)
[![Docs](https://badges.dev.bloomberg.com/badge//Documentation?icon=fa-book)](https://bbgithub.dev.bloomberg.com/pages/python/pystack/)
![Code Style](https://badges.dev.bloomberg.com/badge/code%20style/black/black)

> Tool for analysis of the stack of remote python processes and core files

Pystack is a tool that uses forbidden magic to allow you to inspect the stack frame of a running
Python process or core file in Linux, to know what it is doing without having to interpret nasty
CPython internals.

# What Pystack can do

Pystack has the following amazing features:

- Works with both live processes and core files.
- It can tell you if a thread has the Python GIL, if is waiting for it or if is currently dropping
  it.
- It can tell you if a given thread is garbage collecting.
- Obtain the merged Python/native traceback to better debug and analyze extension modules and
  native code. This means that you will obtain the native stack trace (C/C++ function calls) but
  when the interpreter calls a Python function, the Python name, file and line number will be shown
  at that point instead of the internal C code that the interpreter uses to do such call.
- It can show inlined and overloaded native function calls.
- It can show values of local variables and function arguments of Python stack frames.
- Automatic demangling of symbols.
- Is always safe to use in running processes: Pystack does not modify or execute any code in the
  running process at all: it just read some segments of its memory and the ELF files referenced by
  the memory maps.
- It can perform a Python stack analysis without stopping the process at all.
- Is super fast! It can analyze core files 10x faster than other general-purpose tools like gdb.
- Works with aggressively optimized binaries like the ones we use at Bloomberg for the Python
  interpreters.
- Works with binaries that do not have symbols or DWARF information (Python stack only).
- Self-contained: it does not depend on external tools or programs other than the Python
  interpreter used to run Pystack itself.

# Demo

![pystack demo](https://bbgithub.dev.bloomberg.com/storage/user/9113/files/06c25780-c5df-11ea-984f-99e3320090cb)

# Developing

We welcome contributions to `pystack`. Check
[CONTRIBUTING.md](https://bbgithub.dev.bloomberg.com/python/pystack/blob/main/CONTRIBUTING.md)
to get an idea of how to contribute to the project.
