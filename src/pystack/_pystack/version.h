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

struct py_tuple_v
{
    ssize_t size;
    FieldOffset<Py_ssize_t> o_ob_size;
    FieldOffset<PyObject* [1]> o_ob_item;
};

struct py_list_v
{
    ssize_t size;
    FieldOffset<Py_ssize_t> o_ob_size;
    FieldOffset<PyObject**> o_ob_item;
};

struct py_dict_v
{
    ssize_t size;
    FieldOffset<remote_addr_t> o_ma_keys;
    FieldOffset<remote_addr_t> o_ma_values;
};

struct py_dictkeys_v
{
    ssize_t size;
    FieldOffset<Py_ssize_t> o_dk_size;
    FieldOffset<uint8_t> o_dk_kind;
    FieldOffset<Py_ssize_t> o_dk_nentries;
    FieldOffset<char[1]> o_dk_indices;
};

struct py_dictvalues_v
{
    ssize_t size;
    FieldOffset<remote_addr_t[1]> o_values;
};

struct py_float_v
{
    ssize_t size;
    FieldOffset<double> o_ob_fval;
};

struct py_long_v
{
    ssize_t size;
    FieldOffset<Py_ssize_t> o_ob_size;
    FieldOffset<digit[1]> o_ob_digit;
};

struct py_bytes_v
{
    ssize_t size;
    FieldOffset<Py_ssize_t> o_ob_size;
    FieldOffset<char[1]> o_ob_sval;
};

struct py_unicode_v
{
    ssize_t size;
    FieldOffset<Python3::_PyUnicode_State> o_state;
    FieldOffset<Py_ssize_t> o_length;
    FieldOffset<remote_addr_t> o_ascii;
};

struct py_object_v
{
    ssize_t size;
    FieldOffset<remote_addr_t> o_ob_type;
};

struct py_code_v
{
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
    ssize_t size;
    FieldOffset<remote_addr_t> o_finalizing;
    FieldOffset<remote_addr_t> o_interp_head;
    FieldOffset<char> o_gc;  // Using char because we can only use the offset,
                             // as the size and members change between versions
    FieldOffset<uintptr_t> o_tstate_current;

    FieldOffset<char[8]> o_dbg_off_cookie;
    FieldOffset<uint64_t> o_dbg_off_py_version_hex;
    FieldOffset<uint64_t> o_dbg_off_free_threaded;

    FieldOffset<uint64_t> o_dbg_off_runtime_state_struct_size;
    FieldOffset<uint64_t> o_dbg_off_runtime_state_finalizing;
    FieldOffset<uint64_t> o_dbg_off_runtime_state_interpreters_head;

    FieldOffset<uint64_t> o_dbg_off_interpreter_state_struct_size;
    FieldOffset<uint64_t> o_dbg_off_interpreter_state_id;
    FieldOffset<uint64_t> o_dbg_off_interpreter_state_next;
    FieldOffset<uint64_t> o_dbg_off_interpreter_state_threads_head;
    FieldOffset<uint64_t> o_dbg_off_interpreter_state_gc;
    FieldOffset<uint64_t> o_dbg_off_interpreter_state_imports_modules;
    FieldOffset<uint64_t> o_dbg_off_interpreter_state_sysdict;
    FieldOffset<uint64_t> o_dbg_off_interpreter_state_builtins;
    FieldOffset<uint64_t> o_dbg_off_interpreter_state_ceval_gil;
    FieldOffset<uint64_t> o_dbg_off_interpreter_state_gil_runtime_state;
    FieldOffset<uint64_t> o_dbg_off_interpreter_state_gil_runtime_state_enabled;
    FieldOffset<uint64_t> o_dbg_off_interpreter_state_gil_runtime_state_locked;
    FieldOffset<uint64_t> o_dbg_off_interpreter_state_gil_runtime_state_holder;

    FieldOffset<uint64_t> o_dbg_off_thread_state_struct_size;
    FieldOffset<uint64_t> o_dbg_off_thread_state_prev;
    FieldOffset<uint64_t> o_dbg_off_thread_state_next;
    FieldOffset<uint64_t> o_dbg_off_thread_state_interp;
    FieldOffset<uint64_t> o_dbg_off_thread_state_current_frame;
    FieldOffset<uint64_t> o_dbg_off_thread_state_thread_id;
    FieldOffset<uint64_t> o_dbg_off_thread_state_native_thread_id;
    FieldOffset<uint64_t> o_dbg_off_thread_state_datastack_chunk;
    FieldOffset<uint64_t> o_dbg_off_thread_state_status;

    FieldOffset<uint64_t> o_dbg_off_interpreter_frame_struct_size;
    FieldOffset<uint64_t> o_dbg_off_interpreter_frame_previous;
    FieldOffset<uint64_t> o_dbg_off_interpreter_frame_executable;
    FieldOffset<uint64_t> o_dbg_off_interpreter_frame_instr_ptr;
    FieldOffset<uint64_t> o_dbg_off_interpreter_frame_localsplus;
    FieldOffset<uint64_t> o_dbg_off_interpreter_frame_owner;

    FieldOffset<uint64_t> o_dbg_off_code_object_struct_size;
    FieldOffset<uint64_t> o_dbg_off_code_object_filename;
    FieldOffset<uint64_t> o_dbg_off_code_object_name;
    FieldOffset<uint64_t> o_dbg_off_code_object_qualname;
    FieldOffset<uint64_t> o_dbg_off_code_object_linetable;
    FieldOffset<uint64_t> o_dbg_off_code_object_firstlineno;
    FieldOffset<uint64_t> o_dbg_off_code_object_argcount;
    FieldOffset<uint64_t> o_dbg_off_code_object_localsplusnames;
    FieldOffset<uint64_t> o_dbg_off_code_object_localspluskinds;
    FieldOffset<uint64_t> o_dbg_off_code_object_co_code_adaptive;

    FieldOffset<uint64_t> o_dbg_off_pyobject_struct_size;
    FieldOffset<uint64_t> o_dbg_off_pyobject_ob_type;

    FieldOffset<uint64_t> o_dbg_off_type_object_struct_size;
    FieldOffset<uint64_t> o_dbg_off_type_object_tp_name;
    FieldOffset<uint64_t> o_dbg_off_type_object_tp_repr;
    FieldOffset<uint64_t> o_dbg_off_type_object_tp_flags;

    FieldOffset<uint64_t> o_dbg_off_tuple_object_struct_size;
    FieldOffset<uint64_t> o_dbg_off_tuple_object_ob_item;
    FieldOffset<uint64_t> o_dbg_off_tuple_object_ob_size;

    FieldOffset<uint64_t> o_dbg_off_list_object_struct_size;
    FieldOffset<uint64_t> o_dbg_off_list_object_ob_item;
    FieldOffset<uint64_t> o_dbg_off_list_object_ob_size;

    FieldOffset<uint64_t> o_dbg_off_dict_object_struct_size;
    FieldOffset<uint64_t> o_dbg_off_dict_object_ma_keys;
    FieldOffset<uint64_t> o_dbg_off_dict_object_ma_values;

    FieldOffset<uint64_t> o_dbg_off_float_object_struct_size;
    FieldOffset<uint64_t> o_dbg_off_float_object_ob_fval;

    FieldOffset<uint64_t> o_dbg_off_long_object_struct_size;
    FieldOffset<uint64_t> o_dbg_off_long_object_lv_tag;
    FieldOffset<uint64_t> o_dbg_off_long_object_ob_digit;

    FieldOffset<uint64_t> o_dbg_off_bytes_object_struct_size;
    FieldOffset<uint64_t> o_dbg_off_bytes_object_ob_size;
    FieldOffset<uint64_t> o_dbg_off_bytes_object_ob_sval;

    FieldOffset<uint64_t> o_dbg_off_unicode_object_struct_size;
    FieldOffset<uint64_t> o_dbg_off_unicode_object_state;
    FieldOffset<uint64_t> o_dbg_off_unicode_object_length;
    FieldOffset<uint64_t> o_dbg_off_unicode_object_asciiobject_size;

    FieldOffset<uint64_t> o_dbg_off_gc_struct_size;
    FieldOffset<uint64_t> o_dbg_off_gc_collecting;
};

struct py_type_v
{
    ssize_t size;
    FieldOffset<remote_addr_t> o_tp_name;
    FieldOffset<remote_addr_t> o_tp_repr;
    FieldOffset<unsigned long> o_tp_flags;
};

struct py_is_v
{
    ssize_t size;
    FieldOffset<remote_addr_t> o_next;
    FieldOffset<remote_addr_t> o_tstate_head;
    FieldOffset<char> o_gc;  // Using char because we can only use the offset,
                             // as the size and members change between versions
    FieldOffset<remote_addr_t> o_modules;
    FieldOffset<remote_addr_t> o_sysdict;
    FieldOffset<remote_addr_t> o_builtins;
    FieldOffset<remote_addr_t> o_gil_runtime_state;
};

struct py_gc_v
{
    ssize_t size;
    FieldOffset<remote_addr_t> o_collecting;
};

struct py_cframe_v
{
    ssize_t size;
    FieldOffset<remote_addr_t> current_frame;
};

struct py_gilruntimestate_v
{
    ssize_t size;
    FieldOffset<int> o_locked;
    FieldOffset<remote_addr_t> o_last_holder;
};

struct python_v
{
    py_tuple_v py_tuple;
    py_list_v py_list;
    py_dict_v py_dict;
    py_dictkeys_v py_dictkeys;
    py_dictvalues_v py_dictvalues;
    py_float_v py_float;
    py_long_v py_long;
    py_bytes_v py_bytes;
    py_unicode_v py_unicode;
    py_object_v py_object;
    py_type_v py_type;
    py_code_v py_code;
    py_frame_v py_frame;
    py_thread_v py_thread;
    py_is_v py_is;
    py_runtime_v py_runtime;
    py_gc_v py_gc;
    py_cframe_v py_cframe;
    py_gilruntimestate_v py_gilruntimestate;

    template<typename T>
    inline const T& get() const;
};

#define define_python_v_get_specialization(T)                                                           \
    template<>                                                                                          \
    inline const T##_v& python_v::get<T##_v>() const                                                    \
    {                                                                                                   \
        return T;                                                                                       \
    }

define_python_v_get_specialization(py_tuple);
define_python_v_get_specialization(py_list);
define_python_v_get_specialization(py_dict);
define_python_v_get_specialization(py_dictkeys);
define_python_v_get_specialization(py_dictvalues);
define_python_v_get_specialization(py_float);
define_python_v_get_specialization(py_long);
define_python_v_get_specialization(py_bytes);
define_python_v_get_specialization(py_unicode);
define_python_v_get_specialization(py_object);
define_python_v_get_specialization(py_type);
define_python_v_get_specialization(py_code);
define_python_v_get_specialization(py_frame);
define_python_v_get_specialization(py_thread);
define_python_v_get_specialization(py_is);
define_python_v_get_specialization(py_runtime);
define_python_v_get_specialization(py_gc);
define_python_v_get_specialization(py_cframe);
define_python_v_get_specialization(py_gilruntimestate);

#undef define_python_v_get_specialization

const python_v*
getCPythonOffsets(int major, int minor);

}  // namespace pystack
