#pragma once

#include "code.h"
#include "object.h"

namespace pystack {

constexpr int MAXBLOCKS = 20;

typedef struct
{
    int b_type;
    int b_handler;
    int b_level;
} PyTryBlock;

namespace Python2 {
typedef struct _pyframeobject
{
    PyObject_VAR_HEAD struct _pyframeobject* f_back;
    PyCodeObject* f_code;
    PyObject* f_builtins;
    PyObject* f_globals;
    PyObject* f_locals;
    PyObject** f_valuestack;
    PyObject** f_stacktop;
    PyObject* f_trace;
    PyObject *f_exc_type, *f_exc_value, *f_exc_traceback;
    void* f_tstate;
    int f_lasti;
    int f_lineno;
    int f_iblock;
    PyTryBlock f_blockstack[MAXBLOCKS];
    PyObject* f_localsplus[1];
} PyFrameObject;
}  // namespace Python2

namespace Python3_7 {
typedef struct _pyframeobject
{
    PyObject_VAR_HEAD struct _pyframeobject* f_back;
    PyCodeObject* f_code;
    PyObject* f_builtins;
    PyObject* f_globals;
    PyObject* f_locals;
    PyObject** f_valuestack;
    PyObject** f_stacktop;
    PyObject* f_trace;
    char f_trace_lines;
    char f_trace_opcodes;
    PyObject* f_gen;
    int f_lasti;
    int f_lineno;
    int f_iblock;
    char f_executing;
    PyTryBlock f_blockstack[MAXBLOCKS];
    PyObject* f_localsplus[1];
} PyFrameObject;
}  // namespace Python3_7

namespace Python3_10 {
typedef signed char PyFrameState;

typedef struct _pyframeobject
{
    PyObject_VAR_HEAD struct _pyframeobject* f_back;
    PyCodeObject* f_code;
    PyObject* f_builtins;
    PyObject* f_globals;
    PyObject* f_locals;
    PyObject** f_valuestack;
    PyObject* f_trace;
    int f_stackdepth;
    char f_trace_lines;
    char f_trace_opcodes;
    PyObject* f_gen;
    int f_lasti;
    int f_lineno;
    int f_iblock;
    PyFrameState f_state;
    PyTryBlock f_blockstack[MAXBLOCKS];
    PyObject* f_localsplus[1];
} PyFrameObject;

}  // namespace Python3_10

namespace Python3_11 {
typedef signed char PyFrameState;

typedef struct _interpreter_frame
{
    PyObject* f_func;
    PyObject* f_globals;
    PyObject* f_builtins;
    PyObject* f_locals;
    PyObject* f_code;
    PyObject* frame_obj;
    struct _PyInterpreterFrame* previous;
    _Py_CODEUNIT* f_lasti;
    int stacktop;
    bool is_entry;
    char owner;
    PyObject* localsplus[1];
} PyFrameObject;

}  // namespace Python3_11

typedef union {
    Python2::PyFrameObject v2;
    Python3_7::PyFrameObject v3_7;
    Python3_10::PyFrameObject v3_10;
    Python3_11::PyFrameObject v3_11;
} PyFrameObject;

}  // namespace pystack
