.. note
   You should *NOT* add new change log entries to this file, this
   file is managed by towncrier. You *may* edit previous change logs to
   fix problems like typo corrections or such.

Changelog
=========

.. towncrier release notes start

pystack 1.5.1 (2025-08-18)
--------------------------

Bug Fixes
~~~~~~~~~

- Correct a bug in our GitHub Actions workflow that prevented artifacts for 1.5.0 from being uploaded to PyPI. No changes have been made to the PyStack code itself since 1.5.0 (#250)


pystack 1.5.0 (2025-08-18)
--------------------------

Features
~~~~~~~~

- Add a new ``--native-last`` command line flag to only show the C frames after the last Python frame. (#182)
- Add support for Python 3.14 (#229)
- Support kernels that don't have CONFIG_CROSS_MEMORY_ATTACH set. (#240)


Deprecations and Removals
~~~~~~~~~~~~~~~~~~~~~~~~~

- We no longer provide x86-64 musllinux_1_1 wheels. The manylinux project dropped support for musllinux_1_1 on November 1st, 2024. (#237)
- We no longer provide Python 3.7 wheels. Python 3.7 has been end-of-life since June 2023. (#237)


Bug Fixes
~~~~~~~~~

- Fix incorrect file offset calculation when analyzing ELF files with
  non-standard ELF layouts. Previously, pystack would fail to correctly analyze
  Python binaries that had non-standard ELF layouts (for example when compiled
  with certain linker options). The fix properly accounts for PT_LOAD segment
  mappings when calculating file offsets. (#220)
- Improve handling of core files where we cannot determine the executable. (#221)


pystack 1.4.1 (2024-10-04)
--------------------------

Features
~~~~~~~~

- Improve our ability to debug Python 3.13 processes when the ``_Py_DebugOffsets`` cannot be located by accounting for a structure layout change in CPython 3.13.0rc3 (the reversion of the incremental cyclic garbage collector). (#213)


pystack 1.4.0 (2024-09-11)
--------------------------

Features
~~~~~~~~

- Add support for Python 3.13 (#186)
- Add support for gzip compressed corefiles (#171)
- Add a new ``--version`` command line flag to show the version of pystack (#203)
- Support debugging free-threading (a.k.a. "nogil") Python 3.13 builds. Note that PyStack can't itself be run with ``python3.13t``, it can only attach to a ``python3.13t`` process or core file from another interpreter. (#206)


Bug Fixes
~~~~~~~~~

- Fix a bug that was causing Python scripts executed directly via shebang to report the shell script as the executable. (#184)
- Heap corruption could cause PyStack to fail to generate a stack when ``--locals`` mode was used. This has been fixed by falling back to a reasonable default when attempting to format the repr of a local variable causes a dereference of an invalid pointer. (#194)
- Fix a crash when analysing processes where the eval loop has a shim frame at the bottom of the stack (#204)


pystack 1.3.0 (2023-11-28)
--------------------------

Bug Fixes
~~~~~~~~~

- Add a patch to the bundled elfutils used to create wheels to account for a bug when analysing cores with interleaved segments (#153)
- Removed the unused ``--self`` flag. (#141)
- Fix some instances when identifying the pthread id was failing in systems without GLIBC (#152)
- Fix several some race conditions when stopping threads in multithreaded programs (#155)
- Ensure log messages that contain non-UTF-8 data are not lost (#155)


pystack 1.2.0 (2023-07-31)
--------------------------

Features
~~~~~~~~

- Add support for Python 3.12 (#108)
- Improve the performance of reading memory from running processes (#124)
- Improve the performance of reading memory from core files (#126)


pystack 1.1.0 (2023-07-10)
--------------------------

Bug Fixes
~~~~~~~~~

- Allow building with older elfutils than 0.188 when building with glibc (for musl libc we still need newer versions). (#40)
- Improve error reporting when attaching to a process is forbidden. (#98)
- Fix a crash that could occur under some unusual conditions if elfutils could not unwind the stack. (#101)
- Drop a use of the f-string ``=`` specifier, which wasn't introduced until Python 3.8 (running PyStack with Python 3.7 was failing because of this). (#92)


pystack 1.0.1 (2023-04-11)
--------------------------

No significant changes.


pystack 1.0.0 (2023-04-06)
--------------------------

-  Initial release.
