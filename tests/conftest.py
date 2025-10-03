import shutil

import pytest


def pytest_sessionstart(session):
    if not shutil.which("gcore"):
        pytest.exit("gcore not found (you probably forgot to install gdb)")
