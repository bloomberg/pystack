import os
from unittest.mock import patch

import pytest

from pystack.colors import colored
from pystack.colors import format_colored


@pytest.mark.parametrize(
    "text, color, expected",
    [
        ("Grey color", "grey", "\x1b[30mGrey color\x1b[0m"),
        ("Red color", "red", "\x1b[31mRed color\x1b[0m"),
        ("Green color", "green", "\x1b[32mGreen color\x1b[0m"),
        ("Yellow color", "yellow", "\x1b[33mYellow color\x1b[0m"),
        ("Blue color", "blue", "\x1b[34mBlue color\x1b[0m"),
        ("Magenta color", "magenta", "\x1b[35mMagenta color\x1b[0m"),
        ("Cyan color", "cyan", "\x1b[36mCyan color\x1b[0m"),
        ("White color", "white", "\x1b[37mWhite color\x1b[0m"),
    ],
)
def test_basic_colors(text, color, expected):
    assert format_colored(text, color) == expected


@pytest.mark.parametrize(
    "text, highlight, expected",
    [
        ("Grey color", "on_grey", "\x1b[40mGrey color\x1b[0m"),
        ("Red color", "on_red", "\x1b[41mRed color\x1b[0m"),
        ("Green color", "on_green", "\x1b[42mGreen color\x1b[0m"),
        ("Yellow color", "on_yellow", "\x1b[43mYellow color\x1b[0m"),
        ("Blue color", "on_blue", "\x1b[44mBlue color\x1b[0m"),
        ("Magenta color", "on_magenta", "\x1b[45mMagenta color\x1b[0m"),
        ("Cyan color", "on_cyan", "\x1b[46mCyan color\x1b[0m"),
        ("White color", "on_white", "\x1b[47mWhite color\x1b[0m"),
    ],
)
def test_highlights(text, highlight, expected):
    assert format_colored(text, highlight=highlight) == expected


@pytest.mark.parametrize(
    "text, color, attributes, expected",
    [
        ("Bold grey color", "grey", ["bold"], "\x1b[1m\x1b[30mBold grey color\x1b[0m"),
        ("Dark red color", "red", ["faint"], "\x1b[2m\x1b[31mDark red color\x1b[0m"),
        (
            "Italized red color",
            "red",
            ["italized"],
            "\x1b[3m\x1b[31mItalized red color\x1b[0m",
        ),
        (
            "Underline green color",
            "green",
            ["underline"],
            "\x1b[4m\x1b[32mUnderline green color\x1b[0m",
        ),
        (
            "Blink yellow color",
            "yellow",
            ["blink"],
            "\x1b[5m\x1b[33mBlink yellow color\x1b[0m",
        ),
        (
            "Reversed blue color",
            "blue",
            ["reverse"],
            "\x1b[7m\x1b[34mReversed blue color\x1b[0m",
        ),
        (
            "Concealed Magenta color",
            "magenta",
            ["concealed"],
            "\x1b[8m\x1b[35mConcealed Magenta color\x1b[0m",
        ),
        (
            "Bold underline reverse cyan color",
            "cyan",
            ["bold", "underline", "reverse"],
            "\x1b[7m\x1b[4m\x1b[1m\x1b[36mBold underline reverse cyan color\x1b[0m",
        ),
        (
            "Dark blink concealed white color",
            "white",
            ["faint", "blink", "concealed"],
            "\x1b[8m\x1b[5m\x1b[2m\x1b[37mDark blink concealed white color\x1b[0m",
        ),
    ],
)
def test_attributes(text, color, attributes, expected):
    assert format_colored(text, color, attrs=attributes) == expected


@pytest.mark.parametrize(
    "text, color, highlight, attributes, expected",
    [
        (
            "Underline red on grey color",
            "red",
            "on_grey",
            ["underline"],
            "\x1b[4m\x1b[40m\x1b[31mUnderline red on grey color\x1b[0m",
        ),
        (
            "Reversed green on red color",
            "green",
            "on_red",
            ["reverse"],
            "\x1b[7m\x1b[41m\x1b[32mReversed green on red color\x1b[0m",
        ),
    ],
)
def test_mixed_parameters(text, color, highlight, attributes, expected):
    assert (
        format_colored(text, color, highlight=highlight, attrs=attributes) == expected
    )


@patch.dict(os.environ, {"NO_COLOR": "1"})
def test_colored_with_environment_variable():
    with patch("pystack.colors.format_colored") as format_mock:
        output = colored("some text", "red")
    format_mock.assert_not_called()
    assert output == "some text"


def test_colored_without_environment_variable():
    with patch(
        "pystack.colors.format_colored", return_value="formatted red"
    ) as format_mock, patch("pystack.colors._is_stdout_a_tty", return_value=True):
        output = colored("some_text", "red")
    format_mock.assert_called_once()
    assert output == "formatted red"
