.. note
   You should *NOT* add new change log entries to this file, this
   file is managed by towncrier. You *may* edit previous change logs to
   fix problems like typo corrections or such.

Changelog
=========

.. towncrier release notes start

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
