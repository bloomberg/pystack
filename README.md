<p align="center">
<img src="https://user-images.githubusercontent.com/11718525/226942590-de015c9a-4d5b-4960-9c42-8c1eac0845c1.png" width="70%">
</p>

---

![OS Linux](https://img.shields.io/badge/OS-Linux-blue)
![PyPI - Python Version](https://img.shields.io/pypi/pyversions/pystack)
![PyPI - Implementation](https://img.shields.io/pypi/implementation/pystack)
![PyPI](https://img.shields.io/pypi/v/pystack)
![PyPI - Downloads](https://img.shields.io/pypi/dm/pystack)
[![Tests](https://github.com/bloomberg/pystack/actions/workflows/build.yml/badge.svg)](https://github.com/bloomberg/pystack/actions/workflows/build.yml)
![Code Style](https://img.shields.io/badge/code%20style-black-000000.svg)

# Pystack

> Tool for analysis of the stack of remote python processes and core files

Pystack is a tool that uses forbidden magic to allow you to inspect the stack frame of a running
Python process or core file in Linux, to know what it is doing without having to interpret nasty
CPython internals.

# What Pystack can do

Pystack has the following amazing features:

- 💻 Works with both live processes and core files.
- 🧵 It can tell you if a thread has the Python GIL, if is waiting for it or if
  is currently dropping it.
- 🗑️ It can tell you if a given thread is garbage collecting.
- 🐍 Obtain the merged Python/native traceback to better debug and analyze
  extension modules and native code. This means that you will obtain the native
  stack trace (C/C++ function calls) but when the interpreter calls a Python
  function, the Python name, file and line number will be shown at that point
  instead of the internal C code that the interpreter uses to do such call.
- 📈 It can show inlined and overloaded native function calls.
- 🔍 It can show values of local variables and function arguments of Python
  stack frames.
- 🔍 Automatic demangling of symbols.
- 🔒 Is always safe to use in running processes: Pystack does not modify or
  execute any code in the running process at all: it just read some segments of
  its memory and the ELF files referenced by the memory maps.
- ⚡ It can perform a Python stack analysis without stopping the process at all.
- 🚀 Is super fast! It can analyze core files 10x faster than other
  general-purpose tools like gdb.
- 🎯 Works with aggressively optimized binaries for the Python interpreters.
- 🔍 Works with binaries that do not have symbols or DWARF information (Python
  stack only).
- 💼 Self-contained: it does not depend on external tools or programs other than
  the Python interpreter used to run Pystack itself.

## Building from source

If you wish to build Memray from source you need the following binary dependencies in your system:

- libdw
- libelf

Note that this sometimes both libraries are provided together as part of the `elfutils` package.

Check your package manager on how to install these dependencies (for example `apt-get install libdw-dev libelf-dev` in Debian-based systems).
Note that you may need to teach the compiler where to find the header and library files of the dependencies. 

before installing `pystack`. Check the documentation of your package manager to know the location of the header and library
files for more detailed information.

Once you have the binary dependencies installed, you can clone the repository and follow with the normal building process:

```shell
git clone git@github.com:bloomberg/pystack.git memray
cd pystack
python3 -m venv ../pystack-env/  # just an example, put this wherever you want
source ../pystack-env/bin/activate
python3 -m pip install --upgrade pip
python3 -m pip install -e . -r requirements-test.txt -r requirements-extra.txt
```

This will install Pystack in the virtual environment in development mode (the `-e` of the last `pip install` command).

# Documentation

You can find the latest documentation available [here](https://bloomberg.github.io/pystack/).

# Usage

Pystack can be used for both live processes and core files.

```shell
usage: pystack [-h] [-v] [--no-color] {remote,core} ...

Get python stack trace of a remote process

options:
  -h, --help     show this help message and exit
  -v, --verbose
  --no-color     Deactivate colored output

commands:
  {remote,core}  What should be analyzed by Pystack (use <command> --help for a command-specific help section).
    remote       Analyze a remote process given its PID
    core         Analyze a core dump file given its location and the executable
```

## Analyzing processes

The `remote` command is used to analyze the status of a remote process. The analysis is always done in a safe and
non-intrusive way as no code is loaded in the memory space of the process under analysis and no memory is modified
in the remote process. This makes the analysis using ``pystack`` a great option even for services and application
running in environments where the running process must not be impacted in any way. There are several options available:

```shell
usage: pystack remote [-h] [-v] [--no-color] [--no-block] [--native] [--native-all] [--locals] [--exhaustive] [--self] pid

positional arguments:
  pid            The PID of the remote process

options:
  -h, --help     show this help message and exit
  -v, --verbose
  --no-color     Deactivate colored output
  --no-block     do not block the process when inspecting its memory
  --native       Include the native (C) frames in the resulting stack trace
  --native-all   Include native (C) frames from threads not registered with the interpreter (implies --native)
  --locals       Show local variables for each frame in the stack trace
  --exhaustive   Use all possible methods to obtain the Python stack info (may be slow)
```

To use ``pystack`` you just need to provide the PID of the process:

```shell
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
```

## Analyzing core dumps

The `core` subcommand is used to analyze the status of a core dump file.
Analyzing core files is very similar to analyzing processes but there are some
differences as the core file does not contain the totality of the memory that
was valid when the program was alive.  In most cases this makes no difference
as `pystack` will try to adapt automatically, but in some cases (see other
sections) you need to specify extra command line options. When analyzing cores,
there are several options available:

```shell
usage: pystack core [-h] [-v] [--no-color] [--native] [--native-all] [--locals] [--exhaustive] [--lib-search-path LIB_SEARCH_PATH | --lib-search-root LIB_SEARCH_ROOT] core [executable]

positional arguments:
  core                  The path to the core file
  executable            (Optional) The path to the executable of the core file

options:
  -h, --help            show this help message and exit
  -v, --verbose
  --no-color            Deactivate colored output
  --native              Include the native (C) frames in the resulting stack trace
  --native-all          Include native (C) frames from threads not registered with the interpreter (implies --native)
  --locals              Show local variables for each frame in the stack trace
  --exhaustive          Use all possible methods to obtain the Python stack info (may be slow)
  --lib-search-path LIB_SEARCH_PATH
                        List of paths to search for shared libraries loaded in the core. Paths must be separated by the ':' character
  --lib-search-root LIB_SEARCH_ROOT
                        Root directory to search recursively for shared libraries loaded into the core.
```

To use `pystack` with core files, in most cases you just need to provide the location of the core:

```shell
$ pystack core ./the_core_file
Using executable found in the core file: /opt/bb/bin/python3.8

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
```

# License

Pystack is Apache-2.0 licensed, as found in the [LICENSE](LICENSE) file.

# Code of Conduct

- [Code of Conduct](https://github.com/bloomberg/.github/blob/main/CODE_OF_CONDUCT.md)

This project has adopted a Code of Conduct. If you have any concerns about the Code, or behavior that you have experienced in the project, please contact us at opensource@bloomberg.net.

# Security Policy

- [Security Policy](https://github.com/bloomberg/pystack/security/policy)

If you believe you have identified a security vulnerability in this project, please send an email to the project team at opensource@bloomberg.net, detailing the suspected issue and any methods you've found to reproduce it.

Please do NOT open an issue in the GitHub repository, as we'd prefer to keep vulnerability reports private until we've had an opportunity to review and address them.

# Contributing

We welcome your contributions to help us improve and extend this project!

Below you will find some basic steps required to be able to contribute to the project. If you have any questions about this process or any other aspect of contributing to a Bloomberg open source project, feel free to send an email to opensource@bloomberg.net and we'll get your questions answered as quickly as we can.

## Contribution Licensing

Since this project is distributed under the terms of an [open source license](LICENSE), contributions that you make
are licensed under the same terms. In order for us to be able to accept your contributions,
we will need explicit confirmation from you that you are able and willing to provide them under
these terms, and the mechanism we use to do this is called a Developer's Certificate of Origin
[(DCO)](https://github.com/bloomberg/.github/blob/main/DCO.md). This is very similar to the process used by the Linux(R) kernel, Samba, and many
other major open source projects.

To participate under these terms, all that you must do is include a line like the following as the
last line of the commit message for each commit in your contribution:

    Signed-Off-By: Random J. Developer <random@developer.example.org>

The simplest way to accomplish this is to add `-s` or `--signoff` to your `git commit` command.

You must use your real name (sorry, no pseudonyms, and no anonymous contributions).

## Steps

- Create an Issue, select 'Feature Request', and explain the proposed change.
- Follow the guidelines in the issue template presented to you.
- Submit the Issue.
- Submit a Pull Request and link it to the Issue by including "#<issue number>" in the Pull Request summary.
