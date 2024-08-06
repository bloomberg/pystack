.. note
   You should *NOT* add new change log entries to this file, this
   file is managed by towncrier. You *may* edit previous change logs to
   fix problems like typo corrections or such.

Changelog
=========

.. towncrier release notes start

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
