import os
import sys
from typing import Iterable
from typing import Optional

from .colors import colored
from .types import NativeFrame
from .types import PyCodeObject
from .types import PyFrame
from .types import PyThread
from .types import frame_type


def print_thread(thread: PyThread, native: bool) -> None:
    for line in format_thread(thread, native):
        print(line, file=sys.stdout, flush=True)


def format_frame(frame: PyFrame) -> Iterable[str]:
    code: PyCodeObject = frame.code
    function = colored(code.scope, "yellow")
    yield (
        f'    {colored("(Python)", "green")} File "{code.filename}"'
        f", line {code.location.lineno}, in {function}"
    )
    if os.path.exists(code.filename):
        with open(code.filename, "r") as fp:
            lines = fp.readlines()
        source = lines[code.location.lineno - 1]
        line_start, line_end, col_start, col_end = code.location
        if col_start == col_end == 0:
            yield f"        {source.strip()}"
        else:
            if line_end != line_start:
                col_end = len(source)
            a = source[:col_start]
            b = source[col_start:col_end]
            c = source[col_end:]
            final = f'{a}{colored(b, color="blue")}{c}'
            yield f"        {final.strip()}"

    if frame.arguments:
        yield f"      {colored('Arguments:', attrs=['faint'])}"
        for argument, value in frame.arguments.items():
            normalized_value = repr(value)[1:-1]
            yield f"        {argument}: {normalized_value}"
    if frame.locals:
        yield f"      {colored('Locals:', attrs=['faint'])}"
        for local, value in frame.locals.items():
            normalized_value = repr(value)[1:-1]
            yield f"        {local}: {value}"


def _are_the_stacks_mergeable(thread: PyThread) -> bool:
    eval_frames = (
        frame
        for frame in thread.native_frames
        if frame_type(frame, thread.python_version) == NativeFrame.FrameType.EVAL
    )
    n_eval_frames = sum(1 for _ in eval_frames)
    n_entry_frames = sum(1 for frame in thread.frames if frame.is_entry)
    return n_eval_frames == n_entry_frames


def format_thread(thread: PyThread, native: bool) -> Iterable[str]:
    current_frame: Optional[PyFrame] = thread.frame
    if current_frame is None and not native:
        yield f"The frame stack for thread {thread.tid} is empty"
        return

    thread_name = f" ({thread.name}) " if thread.name else " "
    yield (
        f"Traceback for thread {thread.tid}{thread_name}{thread.status} "
        "(most recent call last):"
    )

    if not (native and _are_the_stacks_mergeable(thread)):
        if native:
            yield "* - Unable to merge native stack due to insufficient native information - *"
        while current_frame is not None:
            yield from format_frame(current_frame)
            current_frame = current_frame.next
    else:
        yield from _format_merged_stacks(thread, current_frame)
    yield ""


def _format_merged_stacks(
    thread: PyThread, current_frame: Optional[PyFrame]
) -> Iterable[str]:
    for frame in thread.native_frames:
        if frame_type(frame, thread.python_version) == NativeFrame.FrameType.EVAL:
            assert current_frame is not None
            yield from format_frame(current_frame)
            current_frame = current_frame.next
            while current_frame and not current_frame.is_entry:
                yield from format_frame(current_frame)
                current_frame = current_frame.next
            continue
        elif frame_type(frame, thread.python_version) == NativeFrame.FrameType.IGNORE:
            continue
        elif frame_type(frame, thread.python_version) == NativeFrame.FrameType.OTHER:
            function = colored(frame.symbol, "yellow")
            yield (
                f'    {colored("(C)", "blue")} File "{frame.path}",'
                f" line {frame.linenumber},"
                f" in {function} ({colored(frame.library, attrs=['faint'])})"
            )
        else:  # pragma: no cover
            raise ValueError(
                f"Invalid frame type: {frame_type(frame, thread.python_version)}"
            )
    return current_frame
