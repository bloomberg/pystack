#pragma once

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>

#include "pycompat.h"

namespace pystack {
typedef unsigned long offset_t;
typedef uintptr_t remote_addr_t;

template<typename T>
struct FieldOffset
{
    typedef T Type;
    offset_t offset;
};

struct py_code_v
{
    typedef PyCodeObject Structure;
    ssize_t size;
    FieldOffset<remote_addr_t> o_filename;
    FieldOffset<remote_addr_t> o_name;
    FieldOffset<remote_addr_t> o_lnotab;
    FieldOffset<unsigned int> o_firstlineno;
    FieldOffset<unsigned int> o_argcount;
    FieldOffset<remote_addr_t> o_varnames;
    FieldOffset<char[1]> o_code_adaptive;
};

struct py_frame_v
{
    typedef PyFrameObject Structure;
    ssize_t size;
    FieldOffset<remote_addr_t> o_back;
    FieldOffset<remote_addr_t> o_code;
    FieldOffset<int> o_lasti;
    FieldOffset<uintptr_t> o_prev_instr;
    FieldOffset<PyObject* [1]> o_localsplus;
    FieldOffset<bool> o_is_entry;
    FieldOffset<char> o_owner;
};

struct py_thread_v
{
    typedef PyThreadState Structure;
    ssize_t size;
    FieldOffset<remote_addr_t> o_prev;
    FieldOffset<remote_addr_t> o_next;
    FieldOffset<remote_addr_t> o_interp;
    FieldOffset<remote_addr_t> o_frame;
    FieldOffset<unsigned long> o_thread_id;
    FieldOffset<unsigned long> o_native_thread_id;
};

struct py_runtime_v
{
    typedef PyRuntimeState Structure;
    ssize_t size;
    FieldOffset<remote_addr_t> o_finalizing;
    FieldOffset<remote_addr_t> o_interp_head;
    FieldOffset<GCRuntimeState> o_gc;
    FieldOffset<uintptr_t> o_tstate_current;
};

struct py_type_v
{
    typedef PyTypeObject Structure;
    ssize_t size;
    FieldOffset<remote_addr_t> o_tp_name;
    FieldOffset<remote_addr_t> o_tp_repr;
    FieldOffset<unsigned long> o_tp_flags;
};

struct py_is_v
{
    typedef PyInterpreterState Structure;
    ssize_t size;
    FieldOffset<remote_addr_t> o_next;
    FieldOffset<remote_addr_t> o_tstate_head;
    FieldOffset<GCRuntimeState> o_gc;
    FieldOffset<remote_addr_t> o_modules;
    FieldOffset<remote_addr_t> o_sysdict;
    FieldOffset<remote_addr_t> o_builtins;
    FieldOffset<remote_addr_t> o_gil_runtime_state;
};

struct py_gc_v
{
    typedef GCRuntimeState Structure;
    ssize_t size;
    FieldOffset<remote_addr_t> o_collecting;
};

struct py_cframe_v
{
    typedef CFrame Structure;
    ssize_t size;
    FieldOffset<remote_addr_t> current_frame;
};

struct python_v
{
    py_type_v py_type;
    py_code_v py_code;
    py_frame_v py_frame;
    py_thread_v py_thread;
    py_is_v py_is;
    py_runtime_v py_runtime;
    py_gc_v py_gc;
    py_cframe_v py_cframe;

    template<typename T>
    inline const T& get() const;
};

#define define_python_v_get_specialization(T)                                                           \
    template<>                                                                                          \
    inline const T##_v& python_v::get<T##_v>() const                                                    \
    {                                                                                                   \
        return T;                                                                                       \
    }

define_python_v_get_specialization(py_type);
define_python_v_get_specialization(py_code);
define_python_v_get_specialization(py_frame);
define_python_v_get_specialization(py_thread);
define_python_v_get_specialization(py_is);
define_python_v_get_specialization(py_runtime);
define_python_v_get_specialization(py_gc);
define_python_v_get_specialization(py_cframe);

#undef define_python_v_get_specialization

const python_v*
getCPythonOffsets(int major, int minor);

}  // namespace pystack
