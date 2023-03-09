import pathlib
from typing import Any
from typing import Optional

DETECTED_EXECUTABLE_NOT_FOUND_TEXT = """\
The executable that was automatically located by pystack doesn't exist.
You can try to provide one manually as the second positional argument to 'pystack core'.
"""
EXECUTABLE_NOT_FOUND_HELP_TEXT = """
The executable used to create the core file could not be automatically
determined from the information present in the core file. If you know
what executable was used to create the core file you can try passing
it as an argument. For example:

    pystack core /path/to/the/core_dump /path/to/the/executable

"""
NOT_ENOUGH_INFORMATION_HELP_TEXT = """
There was not enough information to locate the necessary data structures to
obtain the Python stack trace from the process or the core file. Some scenarios
where this can happen are:

* The Python interpreter is shutting down and the internal structures are not
  available or the memory is being freed.

* The process is embedding Python but the embedding machinery is not activated
  in the process or is stale (this can happen with processes like mod_wsgi).

* If you are analyzing a core file, is possible that the binary and all the
  shared libraries that were used to create the core file are not present in
  the machine you are using to do the analysis. In this case the core may
  report not enough information to resolve the necessary symbols. If you see
  messages reporting missing shared libraries, you can try using the
  `--lib-search-root` or `--lib-search-path` options to indicate locations to
  search for shared libraries. For example:

    $ pystack core $COREFILE $EXECUTABLE --lib-search-root /path/to/shared/libs/

  The `--lib-search-root` option is especially useful for self-contained
  applications that pack all shared libraries as this can be simply pointed
  to the root of the self-contained bundle.

You can still try to call pystack with the `--exhaustive` option to activate
more exhaustive (and slower) methods to obtain the necessary information. This
can take some extra time but it can resolve the Python stack even in some of
the most challenging situations.
"""
INVALID_EXECUTABLE_HELP_TEXT = """
pystack has detected that the provided executable is not an ELF file
(Executable and Linkable Format) and therefore cannot be used to analyze the
core file. This can happen if the program that created the core was executed
using an script with a shebang or any other form of non-ELF file with
executable permissions.

To properly analyze the core file, provide the full path to the *Python*
executable that was used in the original invocation and pass it as the second
argument to the 'pystack core' command. Example:

    $ pystack core $(COREFILE) /full/path/to/the/python/executable

Hint: To know what Python executable was used, check the "Core file
information" section above.
"""

MISSING_EXECUTABLE_MAPS_HELP_TEXT = """
pystack could not determine what memory maps are associated with the
executable. The matching between memory maps and executable is determined by
comparing the file name of the executable and the path associated with the
memory maps.

If you are analyzing a core file, try to provide one of the available maps that
are listed in this error message as the executable to the 'pystack core'
command. Example:

    $ pystack core $(COREFILE) /full/path/to/the/python/executable

If there are no memory maps available it can mean that the executable that has
been provided was not the one that was used to start the process that created
the core. In this case, try to provide the correct executable to the 'pystack
core' command.
"""


class PystackError(Exception):
    ...


class EngineError(PystackError):
    def __init__(
        self,
        *args: Any,
        corefile: Optional[pathlib.Path] = None,
        pid: Optional[int] = None,
        **kwargs: Any,
    ) -> None:
        self.corefile = corefile
        self.pid = pid
        super().__init__(*args, **kwargs)

    def __str__(self) -> str:
        message, *_ = self.args
        return f"Engine error: {message}"


class ProcessNotFound(PystackError, ProcessLookupError):
    ...


class InvalidPythonProcess(PystackError):
    ...


class CoreExecutableNotFound(PystackError):
    HELP_TEXT = EXECUTABLE_NOT_FOUND_HELP_TEXT


class DetectedExecutableNotFound(PystackError):
    HELP_TEXT = DETECTED_EXECUTABLE_NOT_FOUND_TEXT


class NotEnoughInformation(PystackError):
    HELP_TEXT = NOT_ENOUGH_INFORMATION_HELP_TEXT


class InvalidExecutable(PystackError):
    HELP_TEXT = INVALID_EXECUTABLE_HELP_TEXT


class MissingExecutableMaps(PystackError):
    HELP_TEXT = MISSING_EXECUTABLE_MAPS_HELP_TEXT
