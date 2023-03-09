#pragma once

#include "object.h"

namespace pystack {
namespace Python2 {
typedef struct
{
    PyObject_HEAD int co_argcount;
    int co_nlocals;
    int co_stacksize;
    int co_flags;
    PyObject* co_code;
    PyObject* co_consts;
    PyObject* co_names;
    PyObject* co_varnames;
    PyObject* co_freevars;
    PyObject* co_cellvars;
    PyObject* co_filename;
    PyObject* co_name;
    int co_firstlineno;
    PyObject* co_lnotab;
} PyCodeObject;
}  // namespace Python2

namespace Python3_3 {
typedef struct
{
    PyObject_HEAD int co_argcount;
    int co_kwonlyargcount;
    int co_nlocals;
    int co_stacksize;
    int co_flags;
    PyObject* co_code;
    PyObject* co_consts;
    PyObject* co_names;
    PyObject* co_varnames;
    PyObject* co_freevars;
    PyObject* co_cellvars;
    unsigned char* co_cell2arg;
    PyObject* co_filename;
    PyObject* co_name;
    int co_firstlineno;
    PyObject* co_lnotab;
} PyCodeObject;
}  // namespace Python3_3

namespace Python3_6 {
typedef struct
{
    PyObject_HEAD int co_argcount;
    int co_kwonlyargcount;
    int co_nlocals;
    int co_stacksize;
    int co_flags;
    int co_firstlineno;
    PyObject* co_code;
    PyObject* co_consts;
    PyObject* co_names;
    PyObject* co_varnames;
    PyObject* co_freevars;
    PyObject* co_cellvars;
    unsigned char* co_cell2arg;
    PyObject* co_filename;
    PyObject* co_name;
    PyObject* co_lnotab;
} PyCodeObject;
}  // namespace Python3_6

namespace Python3_8 {
typedef struct
{
    PyObject_HEAD int co_argcount;
    int co_posonlyargcount;
    int co_kwonlyargcount;
    int co_nlocals;
    int co_stacksize;
    int co_flags;
    int co_firstlineno;
    PyObject* co_code;
    PyObject* co_consts;
    PyObject* co_names;
    PyObject* co_varnames;
    PyObject* co_freevars;
    PyObject* co_cellvars;
    Py_ssize_t* co_cell2arg;
    PyObject* co_filename;
    PyObject* co_name;
    PyObject* co_lnotab;
} PyCodeObject;
}  // namespace Python3_8

namespace Python3_11 {
typedef uint16_t _Py_CODEUNIT;
typedef struct
{
    PyObject_VAR_HEAD PyObject* co_consts;
    PyObject* co_names;
    PyObject* co_exceptiontable;
    int co_flags;
    short co_warmup;
    short _co_linearray_entry_size;
    int co_argcount;
    int co_posonlyargcount;
    int co_kwonlyargcount;
    int co_stacksize;
    int co_firstlineno;
    int co_nlocalsplus;
    int co_nlocals;
    int co_nplaincellvars;
    int co_ncellvars;
    int co_nfreevars;
    PyObject* co_localsplusnames;
    PyObject* co_localspluskinds;
    PyObject* co_filename;
    PyObject* co_name;
    PyObject* co_qualname;
    PyObject* co_linetable;
    PyObject* co_weakreflist;
    PyObject* _co_code;
    char* _co_linearray;
    int _co_firsttraceable;
    void* co_extra;
    char co_code_adaptive[1];
} PyCodeObject;
}  // namespace Python3_11

typedef union {
    Python2::PyCodeObject v2;
    Python3_3::PyCodeObject v3_3;
    Python3_6::PyCodeObject v3_6;
    Python3_8::PyCodeObject v3_8;
    Python3_11::PyCodeObject v3_11;
} PyCodeObject;

}  // namespace pystack
