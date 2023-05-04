import logging
from unittest.mock import MagicMock

import pytest

from pystack._pystack import _check_interpreter_shutdown  # type: ignore
from pystack._pystack import intercept_runtime_errors  # type: ignore
from pystack.errors import EngineError


def test_intercept_runtime_errors():
    # GIVEN
    @intercept_runtime_errors()
    def callable():
        raise RuntimeError

    # WHEN/THEN
    with pytest.raises(EngineError):
        callable()


@pytest.mark.parametrize("status", [-2, 0, 1])
def test_detecting_active_interpreter(status, caplog):
    # GIVEN
    caplog.set_level(logging.INFO)
    manager = MagicMock()
    manager.interpreter_status.return_value = status
    assert manager.interpreter_status() == status

    # WHEN
    _check_interpreter_shutdown(manager)

    # THEN
    assert caplog.records[0].levelname == "INFO"
    assert caplog.records[0].message == "An active interpreter has been detected"


def test_detecting_finalized_interpreter(caplog):
    # GIVEN
    caplog.set_level(logging.INFO)
    manager = MagicMock()
    manager.interpreter_status.return_value = 2

    # WHEN
    _check_interpreter_shutdown(manager)

    # THEN
    assert caplog.records[0].levelname == "WARNING"
    assert "no Python stack trace" in caplog.records[0].message


def test_failure_to_detect_interpreter_status(caplog):
    # GIVEN
    caplog.set_level(logging.INFO)
    manager = MagicMock()
    manager.interpreter_status.return_value = -1

    # WHEN
    _check_interpreter_shutdown(manager)

    # THEN
    assert caplog.records == []
