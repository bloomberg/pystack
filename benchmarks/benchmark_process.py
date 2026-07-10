from pystack.process import (
    get_python_version_for_core,
    InvalidPythonProcess,
    get_python_version_for_process,
    scan_core_bss_for_python_version,
    scan_process_bss_for_python_version
)
from pystack.maps import VirtualMap
from unittest.mock import Mock
from unittest.mock import mock_open
from unittest.mock import patch
import pytest

RANGE=100

def time_get_python_version_for_core_fallback_bss():
    for i in range(RANGE):
        mapinfo = Mock()

        with patch(
            "pystack.process.scan_core_bss_for_python_version"
        ) as scan_bss_mock, patch(
            "pystack.process.LIBPYTHON_REGEXP"
        ) as libpython_regexp_mock, patch(
            "pystack.process.BINARY_REGEXP"
        ) as binary_regexp_mock, patch(
            "subprocess.check_output"
        ) as subprocess_mock:
            scan_bss_mock.return_value = (3, 8)
            major, minor = get_python_version_for_core("corefile", "executable", mapinfo)

def time_get_python_version_for_core_fallback_no_bss():
    for i in range(RANGE):
        mapinfo = Mock()
        mapinfo.bss = None
        with patch(
            "pystack.process.scan_core_bss_for_python_version"
        ) as scan_bss_mock, patch(
            "pystack.process.LIBPYTHON_REGEXP"
        ) as libpython_regexp_mock, patch(
            "pystack.process.BINARY_REGEXP"
        ) as binary_regexp_mock, patch(
            "subprocess.check_output"
        ) as subprocess_mock:
            match = Mock()
            match.group.side_effect = [3, 8]
            libpython_regexp_mock.match.return_value = match
            major, minor = get_python_version_for_core("corefile", "executable", mapinfo)

def time_get_python_version_for_core_fallback_libpython_regexp():
    # GIVEN
    for i in range(RANGE):
        mapinfo = Mock()

        # WHEN
        with patch(
            "pystack.process.scan_core_bss_for_python_version"
        ) as scan_bss_mock, patch(
            "pystack.process.LIBPYTHON_REGEXP"
        ) as libpython_regexp_mock, patch(
            "pystack.process.BINARY_REGEXP"
        ) as binary_regexp_mock, patch(
            "subprocess.check_output"
        ) as subprocess_mock:
            scan_bss_mock.return_value = None
            match = Mock()
            match.group.side_effect = [3, 8]
            libpython_regexp_mock.match.return_value = match
            major, minor = get_python_version_for_core("corefile", "executable", mapinfo)

def time_get_python_version_for_core_fallback_binary_regexp():
    # GIVEN
    for i in range(RANGE):
        mapinfo = Mock()
        mapinfo.libpython = None

        # WHEN
        with patch(
            "pystack.process.scan_core_bss_for_python_version"
        ) as scan_bss_mock, patch(
            "pystack.process.LIBPYTHON_REGEXP"
        ) as libpython_regexp_mock, patch(
            "pystack.process.BINARY_REGEXP"
        ) as binary_regexp_mock, patch(
            "subprocess.check_output"
        ) as subprocess_mock:
            scan_bss_mock.return_value = None
            libpython_regexp_mock.match.return_value = None
            match = Mock()
            match.group.side_effect = [3, 8]
            binary_regexp_mock.match.return_value = match
            major, minor = get_python_version_for_core("corefile", "executable", mapinfo)

def tim_get_python_version_for_core_fallback_version_regexp():
    # GIVEN
    for i in range(RANGE):
        mapinfo = Mock()

        # WHEN
        with patch(
            "pystack.process.scan_core_bss_for_python_version"
        ) as scan_bss_mock, patch(
            "pystack.process.LIBPYTHON_REGEXP"
        ) as libpython_regexp_mock, patch(
            "pystack.process.BINARY_REGEXP"
        ) as binary_regexp_mock, patch(
            "subprocess.check_output"
        ) as subprocess_mock:
            scan_bss_mock.return_value = None
            libpython_regexp_mock.match.return_value = None
            subprocess_mock.return_value = "Python 3.8.3"
            major, minor = get_python_version_for_core("corefile", "executable", mapinfo)

def time_get_python_version_for_core_fallback_falure():
    # GIVEN
    for i in range(RANGE):
        mapinfo = Mock()

        # WHEN
        with patch(
            "pystack.process.scan_core_bss_for_python_version"
        ) as scan_bss_mock, patch(
            "pystack.process.LIBPYTHON_REGEXP"
        ) as libpython_regexp_mock, patch(
            "pystack.process.BINARY_REGEXP"
        ), patch(
            "subprocess.check_output"
        ) as subprocess_mock:
            scan_bss_mock.return_value = None
            libpython_regexp_mock.match.return_value = None
            subprocess_mock.return_value = ""
            # THEN
            with pytest.raises(InvalidPythonProcess):
                get_python_version_for_core("corefile", "executable", mapinfo)



def time_get_python_version_for_process():
    for i in range(RANGE):
        mapinfo = Mock()

        with patch(
            "pystack.process.scan_process_bss_for_python_version"
        ) as scan_bss_mock, patch(
            "pystack.process.LIBPYTHON_REGEXP"
        ) as libpython_regexp_mock, patch(
            "pystack.process.BINARY_REGEXP"
        ) as binary_regexp_mock, patch(
            "subprocess.check_output"
        ) as subprocess_mock:
            scan_bss_mock.return_value = (3, 8)
            major, minor = get_python_version_for_process(0, mapinfo)

def time_scan_core_bss_for_python_version():
    for i in range(RANGE):
        memory = (
            b"garbagegarbagePython 3.8.3 (default, May 22 2020, 23:30:25)garbagegarbage"
        )
        bss = VirtualMap(
            start=0,
            end=len(memory),
            filesize=len(memory),
            offset=0,
            flags="",
            inode=0,
            device="",
            path=None,
        )
        # WHEN

        with patch("builtins.open", mock_open(read_data=memory)):
            major, minor = scan_core_bss_for_python_version("corefile", bss)

def time_scan_core_bss_for_python_version_failure():
    # GIVEM
    for i in range(RANGE):
        memory = b"garbagegarbagegarbagegarbage"
        bss = VirtualMap(
            start=0,
            end=len(memory),
            filesize=len(memory),
            offset=0,
            flags="",
            inode=0,
            device="",
            path=None,
        )
        # WHEN

        with patch("builtins.open", mock_open(read_data=memory)):
            result = scan_core_bss_for_python_version("corefile", bss)


def time_scan_process_bss_for_python_version():
    for i in range(RANGE):
        memory = (
            b"garbagegarbagePython 3.8.3 (default, May 22 2020, 23:30:25)garbagegarbage"
        )
        bss = Mock()
        # WHEN

        with patch("pystack._pystack.copy_memory_from_address", return_value=memory):
            major, minor = scan_process_bss_for_python_version(0, bss)

def time_scan_process_bss_for_python_version_failure():
    for i in range(RANGE):
        memory = b"garbagegarbagegarbagegarbage"
        bss = Mock()
        # WHEN

        with patch("pystack._pystack.copy_memory_from_address", return_value=memory):
            result = scan_process_bss_for_python_version(0, bss)

