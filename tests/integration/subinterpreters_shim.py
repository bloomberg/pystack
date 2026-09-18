import os
import select
import sys
import time
import traceback


def abort(reason):
    """Terminate this process immediately, with a diagnostic on stderr."""
    sys.stderr.write(f"subinterpreter test program failed: {reason}\n")
    sys.stderr.flush()
    os._exit(1)


def read_n(read_fd, count, *, timeout):
    """Read exactly *count* bytes from *read_fd*."""
    deadline = time.monotonic() + timeout
    data = b""
    while len(data) < count:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            abort(f"only {len(data)} of {count} bytes arrived within {timeout} seconds")
        if not select.select([read_fd], [], [], remaining)[0]:
            continue
        chunk = os.read(read_fd, count - len(data))
        if not chunk:
            abort(f"EOF with {count - len(data)} bytes missing")
        data += chunk
    return data


try:
    from concurrent import interpreters  # type: ignore

    def run_in_new_interpreter(code):
        try:
            interpreters.create().exec(code)
        except BaseException:
            traceback.print_exc(file=sys.stderr)
            abort("Terminating due to the above exception")

except ImportError:
    try:
        import _interpreters  # type: ignore

        def run_in_new_interpreter(code):
            excinfo = _interpreters.exec(_interpreters.create(), code)
            if excinfo is not None:
                abort(
                    getattr(excinfo, "errdisplay", None)
                    or getattr(excinfo, "formatted", None)
                    or repr(excinfo)
                )

    except ImportError:
        import _xxsubinterpreters  # type: ignore

        def run_in_new_interpreter(code):
            try:
                _xxsubinterpreters.run_string(
                    _xxsubinterpreters.create(isolated=False), code
                )
            except BaseException:
                traceback.print_exc(file=sys.stderr)
                abort("Terminating due to the above exception")
