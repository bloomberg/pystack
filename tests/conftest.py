import os
import shutil
from pathlib import Path

import pytest


def pytest_sessionstart(session):
    if not shutil.which("gcore"):
        pytest.exit("gcore not found (you probably forgot to install gdb)")

    # A restrictive Yama ptrace_scope stops pystack from attaching to a
    # running process, so the integration tests hang instead of failing
    # with a useful message.
    ptrace_scope_path = "/proc/sys/kernel/yama/ptrace_scope"
    try:
        if os.getuid() == 0:
            ptrace_scope = "0"
        else:
            ptrace_scope = Path(ptrace_scope_path).read_text().strip()
    except OSError:
        ptrace_scope = "0"
    if ptrace_scope != "0":
        pytest.exit(
            f"ptrace_scope is {ptrace_scope}; pystack cannot attach to a running "
            "process and the tests will hang. Run "
            "'sudo sysctl kernel.yama.ptrace_scope=0' and try again."
        )
