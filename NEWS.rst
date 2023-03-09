.. note
You should *NOT* add new change log entries to this file, this
file is managed by towncrier. You *may* edit previous change logs to
fix problems like typo corrections or such.

.. towncrier release notes start

pystack 2.11.1 (2022-12-09)
=====================================

Bugfixes
--------

- Correctly show permission error messages when failing to attach to remote processes in docker containers. (#267)


pystack 2.11.0 (2022-12-01)
=====================================

Features
--------

- Show BUILD IDs from libraries requested by core files that don't match the BUILD IDs of the existing libraries on the machine used for analysis. (#258)
- Allow to gather the name of the shared object for symbols even if there is no DWARF information available. (#262)


Bugfixes
--------

- Fix a bug that was causing pystack to not show local variables if the option is provided for frames without column information. (#263)


pystack 2.10.1 (2022-09-09)
=====================================

Bugfixes
--------

- The search path for shared libraries is now respected when resolving memory segments from core files. (#255)


pystack 2.10.0 (2022-09-01)
=====================================

Features
--------

- Add the Python 3.10 DPKG and make the default DPKG version use Python 3.10. (#253)


pystack 2.9.0 (2022-08-02)
====================================

Features
--------

- Add support for Python 3.11. (#238)
- Highlight the exact expression being executed in the tracebacks if the interpreter supports extended debugging information. (#242)
- Improve the report when the interpreter has no Python frames currently running but is not yet finished. (#243)


Bugfixes
--------

- Correctly resolve the path and line number of some function calls with no inline information. (#248)


Bloomberg.Pystack 2.8.0 (2021-11-03)
====================================

Bugfixes
--------

- Fix the detection of the `.bss` segment to account for scattered non-contiguous maps. (#222)


Bloomberg.Pystack 2.7.0 (2021-09-03)
====================================

Features
--------

- Gather and display the GC status even if `--native` is not used. (#207)
- Pystack can now obtain the `_PyRuntime` struct from elf data without the need to scan the .bss section. (#214)
- Add support for displaying thread names when printing the traceback. (#216)


Bloomberg.Pystack 2.6.0 (2021-07-16)
====================================

Bugfixes
--------

- Mark the private DPKGs as private dependencies and mark conflicts between them and the canonical DPKG.


Bloomberg.Pystack 2.5.0 (2021-07-14)
====================================

Bugfixes
--------

- Fixed a bug that incorrectly parsed devices with large major/minor numbers when reading from the `/proc/` pseudofilesystem. (#200)
- Correctly set the verbosity if `-v` is passed as a global option. (#202)


Bloomberg.Pystack 2.4.0 (2021-07-09)
====================================

Features
--------

- Added support for Python 3.10. (#191)


Bugfixes
--------

- Don't hard crash when decoding corrupted strings obtained from the core/process. (#196)


Bloomberg.Pystack 2.3.0 (2021-05-20)
====================================

Bugfixes
--------

- Fix a crash that happens if the user passes a file that is not a core file or is a corrupted core file. (#187)


Bloomberg.Pystack 2.2.0 (2021-04-26)
====================================

No significant changes.


Bloomberg.Pystack 2.1.0 (2021-04-20)
====================================

No significant changes.


Bloomberg.Pystack 2.0.0 (2021-04-19)
====================================

Features
--------

- Allow to provide a search path for shared libraries that will allow to analyze core files generated
  in a different machine by providing the new location of the shared libraries that were loaded in
  memory space. (#171)
- To allow better analysis of the interpreter shutdown, allow to execute
  "--native-all" even if there is no intepreter stack active. (#177)
- Improve the error when the automatically located executable doesn't exist. (#179)


Bloomberg.Pystack 1.11.0 (2021-03-25)
=====================================

Bugfixes
--------

- The core file information is now displayed even if the core file command line arguments are over the clipping limit (#170)


Bloomberg.Pystack 1.10.0 (2021-03-19)
=====================================

Features
--------

- Display plenty of extra information (such as the reason for the crash, pid, ppid, guid...) when analyzing core files. (#160)
- Improve reports for core files and processes where the interpreter is in the process of shutting down. (#165)
- Read from the data segment of the linked ELF files when analyzing core files to improve the information displayed when using `--locals` (#167)


Bugfixes
--------

- The `--no-color` option is now taken into account for error messages and exceptions. Using `--no-color` also suppresses non-printable characters in the output (the skulls around the error message). (#164)


Bloomberg.Pystack 1.9.0 (2021-02-24)
====================================

Features
--------

- Improved the logic for locating the binary associated with core files
  when there are no section headers in the core dump. (#156)


Bloomberg.Pystack 1.8.0 (2021-02-23)
====================================

Bugfixes
--------

- Fixed the "--no-color" command line argument to be parsed correctly at the beginning
  of the invocation. (#154)


Bloomberg.Pystack 1.7.0 (2021-01-07)
====================================

Features
--------

- Threads that are garbage collecting are identified in the thread status section. (#149)


Bloomberg.Pystack 1.6.0 (2020-11-09)
====================================

Features
--------

- Add a ``--native-all`` argument to include frames from threads which are not registered
  with the interpreter. (#86)


Bloomberg.Pystack 1.5.0 (2020-10-12)
====================================

Features
--------

- Add support for Python 3.9 (#141)
- Allow to work with theads that have been terminated but not joined. This situation can appear in multithreaded crashed applications with daemon threads when the interpreter is shutting down. (#142)


Bloomberg.Pystack 1.4.0 (2020-10-05)
====================================

Bugfixes
--------

- Fix a bug that was preventing pystack to properly get the Python stack when the first
  thread structure does not have an active Python frame. This scenario can happen when
  a native thread grabs the GIL but does not execute any Python code. (#139)


Bloomberg.Pystack 1.3.0 (2020-09-23)
====================================

Bugfixes
--------

- Do not manually deliver SIGSTOP before attaching in blocking mode. This will
  avoid placing the process in the background when attaching, which was an
  unwanted side-effect for many users. (#136)


Bloomberg.Pystack 1.2.0 (2020-09-18)
====================================

Features
--------

- Added a new `--exhaustive` option for core-file analysis that activates some expensive
  options that can help to find the Python stack trace in some of the most challenging scenarios. (#131)


Bugfixes
--------

- Correctly report the GIL status when is not available (#133)


Bloomberg.Pystack 1.1.0 (2020-09-14)
====================================

Bugfixes
--------

- Handle binary data when retrieving local variables (#123)
- Escape control characters for strings and bytes when printing local variables (#126)


Bloomberg.Pystack 1.0.0 (2020-09-01)
====================================

Features
--------

- Semantically colorize the output for better readability. (#120)
- Add a new option (--locals) that allows to display local variables and
  function arguments of Python stack frames. (#103)


Bugfixes
--------

- Improve the error message when the executable fails to be located. (#113)


Bloomberg.Pystack 0.3.1 (2020-07-31)
====================================

Bugfixes
--------
- Correctly propagate engine errors as `PyStackError` classes.


Bloomberg.Pystack 0.3.0 (2020-07-31)
====================================

Features
--------
- Make the executable an optional argument when processing core files.
- Produce a better error message if the executable is not located.

Bloomberg.Pystack 0.2.6 (2020-07-31)
====================================

Bugfixes
--------
- Convert C++ exceptions to Python when building threads.


Features
--------
- Prepend `0x` to hexadecimal addresses.


Bloomberg.Pystack 0.2.5 (2020-07-27)
====================================

Bugfixes
--------
- Turn on symbol demangling again.


Bloomberg.Pystack 0.2.4 (2020-07-27)
====================================

Features
--------
- Include the shared library in the traceback information.


Bugfixes
--------
- Use the Python version to identify the evaluation frame.


Bloomberg.Pystack 0.2.3 (2020-07-24)
====================================

Features
--------
- Provide better formatting for addresses in the logs.
- Do not fail completely if the stacks are not mergeable.
- Print special message if the Python stack is empty.
- Gather the PID from the core file.

Bug Fixes
---------
- Use all the methods if the scan method is set to `ALL`.


Bloomberg.Pystack 0.2.2 (2020-07-22)
====================================

Features
--------
- Allow to scan corefiles by scanning the `.bss` section.


Bloomberg.Pystack 0.2.1 (2020-07-22)
====================================


Bug Fixes
---------
- Do not fail if the process has no heap or bss section.


Bloomberg.Pystack 0.2.0 (2020-07-20)
====================================


Bug Fixes
---------
- Rework the unwinder to account for incomplete DIE information. When compiling
  code with clang, is possible that it does not include some of the necessary
  information to retrieve efficiently the compilation unit debug information
  entries from the given address. In this case, we need to manually reconstruct
  the missing information in order to retrieve what we need.

Features
--------
- Do not analyze the heap by default.  When analyzing some process that embed
  Python but are not executing Python currently, it may be slow to fail after
  all possible fallbacks if `pystack` ends analyzing a big heap segment. To
  improve the user experience, heap analysis (and other future exhaustive
  methods) should be opt-in, behind an `--exhaustive` flag.


Bloomberg.Pystack 0.1.2 (2020-07-16)
====================================

Features
--------
- Add Python3.7 support

Bloomberg.Pystack 0.1.1 (2020-07-14)
====================================

Bug Fixes
---------
- Do not hard fail if the process does not have bss section.


Bloomberg.Pystack 0.1.0 (2020-07-14)
====================================

Features
--------
- Initial release
