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

struct py_unicode_v
{
    int version;
};

struct py_bytes_v
{
    int version;
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
    py_unicode_v py_unicode;
    py_bytes_v py_bytes;
    py_runtime_v py_runtime;
    py_gc_v py_gc;
};

extern python_v* py_v;
extern int PYTHON_MAJOR_VERSION;
extern int PYTHON_MINOR_VERSION;

void
setVersion(int major, int minor);

template<typename T, typename U, auto PMD0, auto PMD1>
inline auto&
versionedField(const U& py_obj)
{
    offset_t offset = py_v->*PMD0.*PMD1;
    return (*((T*)(((char*)&py_obj) + offset)));
}

template<typename T, auto PMD1>
inline auto&
versionedThreadField(const PyThreadState& py_thread)
{
    return versionedField<T, PyThreadState, &python_v::py_thread, PMD1>(py_thread);
}

template<typename T, auto PMD1>
inline auto&
versionedInterpreterStateField(const PyInterpreterState& py_is)
{
    return versionedField<T, PyInterpreterState, &python_v::py_is, PMD1>(py_is);
}

template<typename T, auto PMD1>
inline auto&
versionedGcStatesField(const GCRuntimeState& py_gc)
{
    return versionedField<T, GCRuntimeState, &python_v::py_gc, PMD1>(py_gc);
}

template<typename T, auto PMD1>
inline auto&
versionedFrameField(const PyFrameObject& py_frame)
{
    return versionedField<T, PyFrameObject, &python_v::py_frame, PMD1>(py_frame);
}

template<typename T, auto PMD1>
inline auto&
versionedCodeField(const PyCodeObject& py_code)
{
    return versionedField<T, PyCodeObject, &python_v::py_code, PMD1>(py_code);
}

template<auto PMD1>
inline auto
versionedCodeOffset()
{
    return py_v->py_code.*PMD1;
}

template<typename T, auto PMD1>
inline auto&
versionedRuntimeField(const PyRuntimeState& py_runtime)
{
    return versionedField<T, PyRuntimeState, &python_v::py_runtime, PMD1>(py_runtime);
}

template<typename T, auto PMD1>
inline auto&
versionedTypeField(const PyTypeObject& py_type)
{
    return versionedField<T, PyTypeObject, &python_v::py_type, PMD1>(py_type);
}

}  // namespace pystack
