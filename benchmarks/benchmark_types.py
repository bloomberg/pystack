import pytest

from pystack.types import SYMBOL_IGNORELIST
from pystack.types import NativeFrame
from pystack.types import PyThread
from pystack.types import frame_type

RANGE=100


def time_frame_type_eval_frame_with_pep_523():
    # GIVEN
    symbols = ["_PyEval_EvalFrameDefault", "_PyEval_EvalFrameDefault.cold.32"]
    versions = [
        (2,7),
        (3,5),
        (3,6),
        (3,7),
        (3,8),
        (3,9)
    ]
    
    for i in range(RANGE):
        for symbol in symbols:
            for version in versions:

                frame = NativeFrame(0x3, symbol, "Python/ceval.c", 123, 0, "library.so")

                # WHEN

                type_ = frame_type(frame, version)

def time_frame_type_eval_frame_without_pep_523():
    # GIVEN

    symbols = ["PyEval_EvalFrameEx", "PyEval_EvalFrameEx.cold.32"]
    versions = [
        (2,7),
        (3,5),
        (3,6),
        (3,7),
        (3,8),
        (3,9)
    ]

    for i in range(RANGE):
        for symbol in symbols:
            for version in versions:
                frame = NativeFrame(
                    0x3, "PyEval_EvalFrameEx", "Python/ceval.c", 123, 0, "library.so"
                )

                # WHEN

                type_ = frame_type(frame, version)

def time_frame_type_eval_machinery_is_ignored():
    # GIVEN
    versions = [
        (2,7),
        (3,5),
        (3,6),
        (3,7),
        (3,8),
        (3,9)
    ]
    for i in range(RANGE):
        for version in versions:
            frame = NativeFrame(
                0x3, "_PyEval_SomeStuff", "Python/ceval.c", 123, 0, "library.so"
            )

            # WHEN

            type_ = frame_type(frame, version)

def time_frame_type_private_python_apis_are_ignored():
    # GIVEN
    versions = [
        (2,7),
        (3,5),
        (3,6),
        (3,7),
        (3,8),
        (3,9)
    ]

    for i in range(RANGE):
        for version in versions:
            frame = NativeFrame(
                0x3, "_PySome_Private_Api", "Python/private.c", 123, 0, "library.so"
            )

            # WHEN

            type_ = frame_type(frame, version)

def time_frame_type_vectorcall_machinery():
    # GIVEN
    versions = [
        (2,7),
        (3,5),
        (3,6),
        (3,7),
        (3,8),
        (3,9)
    ]
    for i in range(RANGE):
        for version in versions:
            frame = NativeFrame(
                0x3, "blablabla_vectorcall_blublublu", "Python/private.c", 123, 0, "library.so"
            )

            # WHEN

            type_ = frame_type(frame, version)

def time_frame_type_explicitly_ignored_symbols_are_ignored():
    # GIVEN
    versions = [
        (2,7),
        (3,5),
        (3,6),
        (3,7),
        (3,8),
        (3,9)
    ]
    symbols = sorted(SYMBOL_IGNORELIST)
    for i in range(RANGE):
        for symbol in symbols:
            for version in versions:
                frame = NativeFrame(0x3, symbol, "Python/private.c", 123, 0, "library.so")

                # WHEN

                type_ = frame_type(frame, version)

def time_gil_states():
    gill_states = [0 ,-1, 1]

    for i in range(RANGE):
        for gil_state in gill_states:

            thread = PyThread(1, None, [], gil_state, 0, (3, 8))

            # WHEN

            state = thread.gil_status

def time_gc_states_with_gil():
    # GIVEN
    gill_states = [0 ,-1, 1]
    for i in range(RANGE):
        for gc_state in gill_states:
            thread = PyThread(1, None, [], 1, gc_state, (3, 8))

            # WHEN

            state = thread.gc_status

def time_gc_states_with_no_gil():
    # GIVEN
    gill_states = [0 ,-1, 1]

    for i in range(RANGE):
        for gc_state in gill_states:
            thread = PyThread(1, None, [], 0, gc_state, (3, 8))

            # WHEN

            state = thread.gc_status

def time_dead_thread():
    # GIVEN

    for i in range(RANGE):
        thread = PyThread(0, None, [], 0, 0, (3, 8))

        # WHEN

        state = thread.status
