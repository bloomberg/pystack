#pragma once

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>

#include "pycompat.h"

namespace pystack {
typedef unsigned long offset_t;

struct py_code_v
{
    ssize_t size;
    offset_t o_filename;
    offset_t o_name;
    offset_t o_lnotab;
    offset_t o_firstlineno;
    offset_t o_argcount;
    offset_t o_varnames;
    offset_t o_code_adaptive;
};

struct py_frame_v
{
    ssize_t size;
    offset_t o_back;
    offset_t o_code;
    offset_t o_lasti;
    offset_t o_localsplus;
    offset_t o_is_entry;
};

struct py_thread_v
{
    ssize_t size;
    offset_t o_prev;
    offset_t o_next;
    offset_t o_interp;
    offset_t o_frame;
    offset_t o_thread_id;
};

struct py_runtime_v
{
    ssize_t size;
    offset_t o_finalizing;
    offset_t o_interp_head;
    offset_t o_gc;
    offset_t o_tstate_current;
};

struct py_type_v
{
    ssize_t size;
    offset_t o_tp_name;
    offset_t o_tp_repr;
    offset_t o_tp_flags;
};

typedef struct
{
    ssize_t size;
    offset_t o_next;
    offset_t o_tstate_head;
    offset_t o_gc;
    offset_t o_modules;
    offset_t o_sysdict;
    offset_t o_builtins;
} py_is_v;

typedef struct
{
    ssize_t size;

    offset_t o_collecting;
} py_gc_v;

struct python_v
{
    py_type_v py_type;
    py_code_v py_code;
    py_frame_v py_frame;
    py_thread_v py_thread;
    py_is_v py_is;
    py_runtime_v py_runtime;
    py_gc_v py_gc;
};

const python_v*
getCPythonOffsets(int major, int minor);

}  // namespace pystack
