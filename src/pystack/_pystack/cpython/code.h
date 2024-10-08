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

namespace Python3_12 {
typedef uint16_t _Py_CODEUNIT;
typedef struct
{
    PyObject_VAR_HEAD PyObject* co_consts;
    PyObject* co_names;
    PyObject* co_exceptiontable;
    int co_flags;
    int co_argcount;
    int co_posonlyargcount;
    int co_kwonlyargcount;
    int co_stacksize;
    int co_firstlineno;
    int co_nlocalsplus;
    int co_framesize;
    int co_nlocals;
    int co_ncellvars;
    int co_nfreevars;
    uint32_t co_version;
    PyObject* co_localsplusnames;
    PyObject* co_localspluskinds;
    PyObject* co_filename;
    PyObject* co_name;
    PyObject* co_qualname;
    PyObject* co_linetable;
    PyObject* co_weakreflist;
    void* _co_cached;
    uint64_t _co_instrumentation_version;
    void* _co_monitoring;
    int _co_firsttraceable;
    void* co_extra;
    char co_code_adaptive[1];
} PyCodeObject;
}  // namespace Python3_12

namespace Python3_13 {
typedef uint16_t _Py_CODEUNIT;
typedef struct
{
    PyObject_VAR_HEAD;
    PyObject* co_consts;
    PyObject* co_names;
    PyObject* co_exceptiontable;
    int co_flags;
    int co_argcount;
    int co_posonlyargcount;
    int co_kwonlyargcount;
    int co_stacksize;
    int co_firstlineno;
    int co_nlocalsplus;
    int co_framesize;
    int co_nlocals;
    int co_ncellvars;
    int co_nfreevars;
    uint32_t co_version;
    PyObject* co_localsplusnames;
    PyObject* co_localspluskinds;
    PyObject* co_filename;
    PyObject* co_name;
    PyObject* co_qualname;
    PyObject* co_linetable;
    PyObject* co_weakreflist;
    void* co_executors;
    void* _co_cached;
    uintptr_t _co_instrumentation_version;
    void* _co_monitoring;
    int _co_firsttraceable;
    void* co_extra;
    char co_code_adaptive[1];
} PyCodeObject;
}  // namespace Python3_13

}  // namespace pystack
