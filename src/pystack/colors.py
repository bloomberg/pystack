import os
import re
import sys
from typing import Iterable
from typing import Optional

ATTRIBUTES = dict(
    list(
        zip(
            [
                "bold",
                "faint",
                "italized",
                "underline",
                "blink",
                "",
                "reverse",
                "concealed",
            ],
            list(range(1, 9)),
        )
    )
)
del ATTRIBUTES[""]

ATTRIBUTES_RE = r"\033\[(?:%s)m" % "|".join(["%d" % v for v in ATTRIBUTES.values()])

HIGHLIGHTS = dict(
    list(
        zip(
            [
                "on_grey",
                "on_red",
                "on_green",
                "on_yellow",
                "on_blue",
                "on_magenta",
                "on_cyan",
                "on_white",
            ],
            list(range(40, 48)),
        )
    )
)

HIGHLIGHTS_RE = r"\033\[(?:%s)m" % "|".join(["%d" % v for v in HIGHLIGHTS.values()])

COLORS = dict(
    list(
        zip(
            [
                "grey",
                "red",
                "green",
                "yellow",
                "blue",
                "magenta",
                "cyan",
                "white",
            ],
            list(range(30, 38)),
        )
    )
)

COLORS_RE = r"\033\[(?:%s)m" % "|".join(["%d" % v for v in COLORS.values()])

RESET = "\033[0m"
RESET_RE = r"033\[0m"


def _is_stdout_a_tty() -> bool:
    return hasattr(sys.stdout, "isatty") and sys.stdout.isatty()


def colored(
    text: str,
    color: Optional[str] = None,
    highlight: Optional[str] = None,
    attrs: Optional[Iterable[str]] = None,
) -> str:
    """Colorize text, while stripping nested ANSI color sequences.
    Available text colors:
        red, green, yellow, blue, magenta, cyan, white.
    Available text highlights:
        on_red, on_green, on_yellow, on_blue, on_magenta, on_cyan, on_white.
    Available attributes:
        bold, dark, underline, blink, reverse, concealed.
    Example:
        colored('Hello, World!', 'red', 'on_grey', ['blue', 'blink'])
        colored('Hello, World!', 'green')
    """

    def terminal_supports_color() -> bool:
        if os.getenv("NO_COLOR") is not None:
            return False
        return _is_stdout_a_tty()

    if not terminal_supports_color():
        return text
    return format_colored(text, color, highlight, attrs)


def format_colored(
    text: str,
    color: Optional[str] = None,
    highlight: Optional[str] = None,
    attrs: Optional[Iterable[str]] = None,
) -> str:
    fmt_str = "\033[%dm%s"
    if color is not None:
        text = re.sub(COLORS_RE + "(.*?)" + RESET_RE, r"\1", text)
        text = fmt_str % (COLORS[color], text)
    if highlight is not None:
        text = re.sub(HIGHLIGHTS_RE + "(.*?)" + RESET_RE, r"\1", text)
        text = fmt_str % (HIGHLIGHTS[highlight], text)
    if attrs is not None:
        text = re.sub(ATTRIBUTES_RE + "(.*?)" + RESET_RE, r"\1", text)
        for attr in attrs:
            text = fmt_str % (ATTRIBUTES[attr], text)
    return text + RESET
