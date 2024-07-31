import re
from pathlib import Path

import pytest

from pystack.engine import get_process_threads
from pystack.engine import get_process_threads_for_core
from tests.utils import ALL_PYTHONS
from tests.utils import generate_core_file
from tests.utils import spawn_child_process

TEST_SINGLE_THREAD_FILE = Path(__file__).parent / "single_thread_program.py"
TEST_MULTIPLE_THREADS_FILE = Path(__file__).parent / "multiple_thread_program.py"


def get_threads_for_process(python, script, tmpdir, locals=True):
    with spawn_child_process(python, script, tmpdir) as child_process:
        return list(get_process_threads(child_process.pid, locals=locals))


def get_threads_for_core(python, script, tmpdir, locals=True):
    with generate_core_file(python, script, tmpdir) as core_file:
        return list(get_process_threads_for_core(core_file, python, locals=locals))


ALL_SOURCES = pytest.mark.parametrize(
    "generate_threads",
    [get_threads_for_process, get_threads_for_core],
    ids=["Process", "Core file"],
)

MAX_OUTPUT_LEN = 80


def find_frame(frames, name):
    for frame in frames:
        if frame.code.scope == name:
            return frame
    assert False, f"Frame {name!r} not found in {frames}"  # pragma: no cover


@pytest.mark.parametrize(
    "argument",
    [
        None,
        True,
        False,
        1234,
        1234567890123456,
        "Hello world",
        3.14,
        [1, 2, 3, [1, 2, [1]]],
        (1, 2, 3, (1, 2, [1])),
        {1: 2, 3: 4, "Hello": 3.14, (1, 2): [3, 4]},
        {"a": "b", "c": "d", "e": "f"},
    ],
    ids=[
        "None",
        "True",
        "False",
        "small integer",
        "big_integer",
        "string",
        "float",
        "list",
        "tuple",
        "dict",
        "unicode_dict",
    ],
)
@ALL_PYTHONS
@ALL_SOURCES
def test_common_types(generate_threads, argument, python, tmpdir):
    # GIVEN
    _, python_executable = python

    program_template = """
import sys
import time


def first_func():
    the_local = {argument}
    second_func({argument})

def second_func(the_argument):
    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)

first_func()
    """

    script = tmpdir / "the_script.py"
    script.write_text(
        program_template.format(argument=repr(argument)), encoding="utf-8"
    )

    # WHEN

    threads = generate_threads(python_executable, script, tmpdir)

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 3

    first_func_frame = find_frame(frames, "first_func")
    assert "the_local" in first_func_frame.locals
    assert eval(first_func_frame.locals["the_local"]) == argument

    second_func_frame = find_frame(frames, "second_func")
    assert "the_argument" in second_func_frame.arguments
    assert eval(second_func_frame.arguments["the_argument"]) == argument


@ALL_PYTHONS
@ALL_SOURCES
def test_bytes(generate_threads, python, tmpdir):
    # GIVEN
    _, python_executable = python

    program_template = """
import sys
import time


def first_func():
    the_local = b"Hello world"
    second_func(the_local)

def second_func(the_argument):
    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)

first_func()
    """

    script = tmpdir / "the_script.py"
    script.write_text(program_template, encoding="utf-8")

    # WHEN

    threads = generate_threads(python_executable, script, tmpdir)

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 3

    first_func_frame = find_frame(frames, "first_func")
    assert "the_local" in first_func_frame.locals
    assert "Hello world" in first_func_frame.locals["the_local"]

    second_func_frame = find_frame(frames, "second_func")
    assert "the_argument" in second_func_frame.arguments
    assert "Hello world" in second_func_frame.arguments["the_argument"]


@ALL_PYTHONS
@ALL_SOURCES
def test_bytes_with_binary_data(generate_threads, python, tmpdir):
    # GIVEN
    _, python_executable = python

    program_template = b"""
import sys
import time


def first_func():
    the_local = b"\\xFF"
    second_func(the_local)

def second_func(the_argument):
    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)

first_func()
    """

    script = tmpdir / "the_script.py"
    script.write_binary(program_template)

    # WHEN

    threads = generate_threads(python_executable, script, tmpdir)

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 3

    first_func_frame = find_frame(frames, "first_func")
    assert "the_local" in first_func_frame.locals
    assert first_func_frame.locals["the_local"] == "<BINARY>"

    second_func_frame = find_frame(frames, "second_func")
    assert "the_argument" in second_func_frame.arguments
    assert second_func_frame.arguments["the_argument"] == "<BINARY>"


@ALL_PYTHONS
@ALL_SOURCES
def test_split_dict(generate_threads, python, tmpdir):
    # GIVE
    _, python_executable = python

    program_template = """
import sys
import time

class A:
   def __init__(self, x, y):
       self.x = x
       self.y = y

a = A(1, "barbaridad")

def first_func():
    the_local = a.__dict__
    second_func(a.__dict__)

def second_func(the_argument):
    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)

first_func()
    """

    script = tmpdir / "the_script.py"
    script.write_text(program_template, encoding="utf-8")

    # WHEN

    threads = generate_threads(python_executable, script, tmpdir)

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 3

    first_func_frame = find_frame(frames, "first_func")
    assert "the_local" in first_func_frame.locals
    assert eval(first_func_frame.locals["the_local"]) == {
        "x": 1,
        "y": "barbaridad",
    }

    second_func_frame = find_frame(frames, "second_func")
    assert "the_argument" in second_func_frame.arguments
    assert eval(second_func_frame.arguments["the_argument"]) == {
        "x": 1,
        "y": "barbaridad",
    }


@ALL_PYTHONS
@ALL_SOURCES
def test_dict_with_dummy_entries(generate_threads, python, tmpdir):
    # GIVEN
    _, python_executable = python

    program_template = """
import sys
import time

def first_func():
    some_dict = {x:x for x in range(10)}
    del some_dict[0]
    del some_dict[1]
    del some_dict[2]
    some_dict[1] = 42
    the_local = some_dict
    second_func(some_dict)

def second_func(the_argument):
    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)

first_func()
    """

    script = tmpdir / "the_script.py"
    script.write_text(program_template, encoding="utf-8")

    # WHEN

    threads = generate_threads(python_executable, script, tmpdir)

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 3

    first_func_frame = find_frame(frames, "first_func")
    assert "the_local" in first_func_frame.locals
    assert eval(first_func_frame.locals["the_local"]) == {
        1: 42,
        3: 3,
        4: 4,
        5: 5,
        6: 6,
        7: 7,
        8: 8,
        9: 9,
    }

    second_func_frame = find_frame(frames, "second_func")
    assert "the_argument" in second_func_frame.arguments
    assert eval(second_func_frame.arguments["the_argument"]) == {
        1: 42,
        3: 3,
        4: 4,
        5: 5,
        6: 6,
        7: 7,
        8: 8,
        9: 9,
    }


@ALL_PYTHONS
@ALL_SOURCES
def test_gigantic_integer(generate_threads, python, tmpdir):
    # GIVEN
    _, python_executable = python

    program_template = """
import sys
import time

def first_func():
    the_local = 999999999999999999999999
    second_func(the_local)

def second_func(the_argument):
    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)

first_func()
    """

    script = tmpdir / "the_script.py"
    script.write_text(program_template, encoding="utf-8")

    # WHEN

    threads = generate_threads(python_executable, script, tmpdir)

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 3

    first_func_frame = find_frame(frames, "first_func")
    assert "the_local" in first_func_frame.locals
    assert first_func_frame.locals["the_local"] == "<UNRESOLVED BIG INT>"

    second_func_frame = find_frame(frames, "second_func")
    assert "the_argument" in second_func_frame.arguments
    assert second_func_frame.arguments["the_argument"] == "<UNRESOLVED BIG INT>"


@ALL_PYTHONS
@ALL_SOURCES
def test_custom_object(generate_threads, python, tmpdir):
    # GIVEN
    _, python_executable = python

    program_template = """
import sys
import time

class A:
    pass

def first_func():
    the_local = A()
    second_func(the_local)

def second_func(the_argument):
    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)

first_func()
    """

    script = tmpdir / "the_script.py"
    script.write_text(program_template, encoding="utf-8")

    # WHEN

    threads = generate_threads(python_executable, script, tmpdir)

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 3

    repr_regex = re.compile(r"<(?:A|instance|\?\?\?) at 0[xX][0-9a-fA-F]+>")

    first_func_frame = find_frame(frames, "first_func")
    assert "the_local" in first_func_frame.locals
    argument_repr = first_func_frame.locals["the_local"]
    assert repr_regex.match(argument_repr)

    second_func_frame = find_frame(frames, "second_func")
    assert "the_argument" in second_func_frame.arguments
    argument_repr = second_func_frame.arguments["the_argument"]
    assert repr_regex.match(argument_repr)


@pytest.mark.parametrize(
    "argument",
    [
        "list(range(50))",
        "tuple(range(50))",
        "{x:x for x in range(50)}",
        "'a' * 100",
        "x = [1, 2, 3]; x.append(x)",
        "x = {1:2, 3:4}; x[2] = x",
    ],
    ids=[
        "Big list",
        "Big tuple",
        "Big dict",
        "Big string",
        "Recursive List",
        "Recursive Dict",
    ],
)
@ALL_PYTHONS
@ALL_SOURCES
def test_output_limiting(generate_threads, argument, python, tmpdir):
    # GIVEN
    _, python_executable = python

    program_template = """
import sys
import time

VAR = {argument}

def first_func():
    the_local = VAR
    second_func(VAR)

def second_func(the_argument):
    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)

first_func()
    """

    script = tmpdir / "the_script.py"
    script.write_text(program_template.format(argument=argument), encoding="utf-8")

    # WHEN

    threads = generate_threads(python_executable, script, tmpdir)

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 3

    first_func_frame = find_frame(frames, "first_func")
    assert "the_local" in first_func_frame.locals
    assert len(first_func_frame.locals["the_local"]) <= MAX_OUTPUT_LEN
    assert "..." in first_func_frame.locals["the_local"]

    second_func_frame = find_frame(frames, "second_func")
    assert "the_argument" in second_func_frame.arguments
    assert len(second_func_frame.arguments["the_argument"]) <= MAX_OUTPUT_LEN
    assert "..." in second_func_frame.arguments["the_argument"]


@ALL_PYTHONS
@ALL_SOURCES
def test_locals_not_resolved(generate_threads, python, tmpdir):
    # GIVEN
    _, python_executable = python

    program_template = """
import sys
import time


def first_func():
    the_local = "the_local"
    second_func(the_local)

def second_func(the_argument):
    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)

first_func()
    """

    script = tmpdir / "the_script.py"
    script.write_text(program_template, encoding="utf-8")

    # WHEN
    threads = generate_threads(python_executable, script, tmpdir, locals=False)

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 3

    first_func_frame = find_frame(frames, "first_func")
    assert "the_local" not in first_func_frame.locals

    second_func_frame = find_frame(frames, "second_func")
    assert "the_argument" not in second_func_frame.arguments


@ALL_PYTHONS
@ALL_SOURCES
def test_trashed_locals(generate_threads, python, tmpdir):
    # GIVEN
    _, python_executable = python

    program_template = """
import ctypes
import sys
import time

class ListObject(ctypes.Structure):
    _fields_ = [
        ("ob_refcnt", ctypes.c_ssize_t),
        ("ob_type", ctypes.c_void_p),
        ("ob_size", ctypes.c_ssize_t),
        ("ob_item", ctypes.c_void_p),
    ]

class TupleObject(ctypes.Structure):
    _fields_ = [
        ("ob_refcnt", ctypes.c_ssize_t),
        ("ob_type", ctypes.c_void_p),
        ("ob_size", ctypes.c_ssize_t),
        ("ob_item0", ctypes.c_void_p),
        ("ob_item1", ctypes.c_void_p),
    ]

def main():
    bad_type = (1, 2, 3)
    bad_elem = (4, 5, 6)
    nullelem = (7, 8, 9)
    bad_list = [0, 1, 2]

    TupleObject.from_address(id(bad_type)).ob_type = 0xded
    TupleObject.from_address(id(bad_elem)).ob_item1 = 0xbad
    TupleObject.from_address(id(nullelem)).ob_item1 = 0x0
    ListObject.from_address(id(bad_list)).ob_item = 0x0

    fifo = sys.argv[1]
    with open(sys.argv[1], "w") as fifo:
        fifo.write("ready")
    time.sleep(1000)

main()
    """

    script = tmpdir / "the_script.py"
    script.write_text(program_template, encoding="utf-8")

    # WHEN
    threads = generate_threads(python_executable, script, tmpdir, locals=True)

    # THEN

    assert len(threads) == 1
    (thread,) = threads

    frames = list(thread.frames)
    assert (len(frames)) == 2

    main = find_frame(frames, "main")
    assert re.match(r"^<invalid object at 0x[0-9a-f]{4,}>$", main.locals["bad_type"])
    assert main.locals["bad_elem"] == "(4, <invalid object at 0xbad>, 6)"
    assert main.locals["nullelem"] == "(7, <invalid object at 0x0>, 9)"
    assert re.match(r"^<list object at 0x[0-9a-f]{4,}>$", main.locals["bad_list"])
