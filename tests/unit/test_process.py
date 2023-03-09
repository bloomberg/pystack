from unittest.mock import Mock
from unittest.mock import mock_open
from unittest.mock import patch

import pytest

from pystack.errors import InvalidPythonProcess
from pystack.maps import VirtualMap
from pystack.process import BINARY_REGEXP
from pystack.process import LIBPYTHON_REGEXP
from pystack.process import VERSION_REGEXP
from pystack.process import get_python_version_for_core
from pystack.process import get_python_version_for_process
from pystack.process import scan_core_bss_for_python_version
from pystack.process import scan_process_bss_for_python_version


@pytest.mark.parametrize(
    "text, version",
    [
        ("libpython3.8.so", (3, 8)),
        ("libpython3.5.12.so", (3, 5)),
        ("libpython3.8m.so", (3, 8)),
        ("libpython3.8d.so", (3, 8)),
        ("libpython3.8dm.so", (3, 8)),
        ("libpython2.7.so.1", (2, 7)),
        ("libpython2.7.so.1.0", (2, 7)),
        ("LIBPYTHON3.8.so", (3, 8)),
        ("LiBpYtHoN3.6.so", (3, 6)),
    ],
)
def test_libpython_detection(text, version):
    # GIVEN / WHEN
    result = LIBPYTHON_REGEXP.match(text)

    # THEN
    assert result

    major, minor = version
    assert int(result.group("major")) == major
    assert int(result.group("minor")) == minor


@pytest.mark.parametrize(
    "text", ["libpython.so", "libpython.so.1.0", "libpythondm.so.1.0"]
)
def test_libpython_false_cases(text):
    # GIVEN / WHEN
    result = LIBPYTHON_REGEXP.match(text)

    # THEN
    assert result is None


@pytest.mark.parametrize(
    "text, version",
    [
        ("python3.8", (3, 8)),
        ("python3.5.1.2", (3, 5)),
        ("python2.7.exe", (2, 7)),
        ("Python3.6", (3, 6)),
        ("PyThOn3.5", (3, 5)),
    ],
)
def test_executable_detection(text, version):
    # GIVEN / WHEN
    result = BINARY_REGEXP.match(text)

    # THEN
    assert result

    major, minor = version
    assert int(result.group("major")) == major
    assert int(result.group("minor")) == minor


@pytest.mark.parametrize("text", ["cat3.8", "python3", "python2"])
def test_executable_false_cases(text):
    # GIVEN / WHEN
    result = BINARY_REGEXP.match(text)

    # THEN
    assert result is None


@pytest.mark.parametrize(
    "text, version",
    [
        ("Python 3.8.2", (3, 8)),
        ("Python 3.8.1rc2", (3, 8)),
        ("Python 3.9.0b4", (3, 9)),
        ("Python 2.7.16", (2, 7)),
    ],
)
def test_version_detection(text, version):
    # GIVEN / WHEN
    result = VERSION_REGEXP.match(text)

    # THEN
    assert result

    major, minor = version
    assert int(result.group("major")) == major
    assert int(result.group("minor")) == minor


def test_get_python_version_for_process_fallback_bss():
    # GIVEN
    mapinfo = Mock()

    # WHEN
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

    # THEN
    assert (major, minor) == (3, 8)
    scan_bss_mock.assert_called_once()
    libpython_regexp_mock.match.assert_not_called()
    binary_regexp_mock.assert_not_called()
    subprocess_mock.assert_not_called()


def test_get_python_version_for_process_fallback_libpython_regexp():
    # GIVEN
    mapinfo = Mock()

    # WHEN
    with patch(
        "pystack.process.scan_process_bss_for_python_version"
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
        major, minor = get_python_version_for_process(0, mapinfo)

    # THEN
    assert (major, minor) == (3, 8)
    scan_bss_mock.assert_called_once()
    libpython_regexp_mock.match.assert_called_once()
    binary_regexp_mock.assert_not_called()
    subprocess_mock.assert_not_called()


def test_get_python_version_for_process_fallback_binary_regexp():
    # GIVEN
    mapinfo = Mock()
    mapinfo.libpython = None

    # WHEN
    with patch(
        "pystack.process.scan_process_bss_for_python_version"
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
        major, minor = get_python_version_for_process(0, mapinfo)

    # THEN
    assert (major, minor) == (3, 8)
    scan_bss_mock.assert_called_once()
    libpython_regexp_mock.match.assert_not_called()
    binary_regexp_mock.match.assert_called_once()
    subprocess_mock.assert_not_called()


def test_get_python_version_for_process_fallback_version_regexp():
    # GIVEN
    mapinfo = Mock()

    # WHEN
    with patch(
        "pystack.process.scan_process_bss_for_python_version"
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
        major, minor = get_python_version_for_process(0, mapinfo)

    # THEN
    assert (major, minor) == (3, 8)
    scan_bss_mock.assert_called_once()
    libpython_regexp_mock.match.assert_called_once()
    binary_regexp_mock.match.asser_not_called()
    subprocess_mock.assert_called_once()


def test_get_python_version_for_process_fallback_failure():
    # GIVEN
    mapinfo = Mock()

    # WHEN
    with patch(
        "pystack.process.scan_process_bss_for_python_version"
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
            get_python_version_for_process(0, mapinfo)


def test_get_python_version_for_core_fallback_bss():
    # GIVEN
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

        scan_bss_mock.return_value = (3, 8)
        major, minor = get_python_version_for_core("corefile", "executable", mapinfo)

    # THEN
    assert (major, minor) == (3, 8)
    scan_bss_mock.assert_called_once()
    libpython_regexp_mock.match.assert_not_called()
    binary_regexp_mock.assert_not_called()
    subprocess_mock.assert_not_called()


def test_get_python_version_for_core_fallback_libpython_regexp():
    # GIVEN
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

    # THEN
    assert (major, minor) == (3, 8)
    scan_bss_mock.assert_called_once()
    libpython_regexp_mock.match.assert_called_once()
    binary_regexp_mock.assert_not_called()
    subprocess_mock.assert_not_called()


def test_get_python_version_for_core_fallback_binary_regexp():
    # GIVEN
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

    # THEN
    assert (major, minor) == (3, 8)
    scan_bss_mock.assert_called_once()
    libpython_regexp_mock.match.assert_not_called()
    binary_regexp_mock.match.assert_called_once()
    subprocess_mock.assert_not_called()


def test_get_python_version_for_core_fallback_version_regexp():
    # GIVEN
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

    # THEN
    assert (major, minor) == (3, 8)
    scan_bss_mock.assert_called_once()
    libpython_regexp_mock.match.assert_called_once()
    binary_regexp_mock.match.asser_not_called()
    subprocess_mock.assert_called_once()


def test_get_python_version_for_core_fallback_falure():
    # GIVEN
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


def test_scan_process_bss_for_python_version():
    # GIVEM

    memory = (
        b"garbagegarbagePython 3.8.3 (default, May 22 2020, 23:30:25)garbagegarbage"
    )
    bss = Mock()
    # WHEN

    with patch("pystack._pystack.copy_memory_from_address", return_value=memory):
        major, minor = scan_process_bss_for_python_version(0, bss)

    # THEN

    assert major == 3
    assert minor == 8


def test_scan_process_bss_for_python_version_failure():
    # GIVEM

    memory = b"garbagegarbagegarbagegarbage"
    bss = Mock()
    # WHEN

    with patch("pystack._pystack.copy_memory_from_address", return_value=memory):
        result = scan_process_bss_for_python_version(0, bss)

    # THEN

    assert result is None


def test_scan_core_bss_for_python_version():
    # GIVEM

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

    # THEN

    assert major == 3
    assert minor == 8


def test_scan_core_bss_for_python_version_failure():
    # GIVEM

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

    # THEN

    assert result is None
