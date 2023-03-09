#pragma once

#include "gc.h"
#include "object.h"

namespace pystack {

struct _ts; /* Forward */
struct _is; /* Forward */

typedef void* PyThread_type_lock;

typedef struct _Py_atomic_int
{
    int _value;
} _Py_atomic_int;

struct _pending_calls
{
    PyThread_type_lock lock;
    _Py_atomic_int calls_to_do;
    int async_exc;
#define NPENDINGCALLS 32
    struct
    {
        int (*func)(void*);
        void* arg;
    } calls[NPENDINGCALLS];
    int first;
    int last;
};

struct _ceval_state
{
    int recursion_limit;
    int tracing_possible;
    _Py_atomic_int eval_breaker;
    _Py_atomic_int gil_drop_request;
    struct _pending_calls pending;
};

namespace Python2 {
typedef struct _is
{
    struct _is* next;
    struct _ts* tstate_head;
    PyObject* modules;
    PyObject* sysdict;
    PyObject* builtins;
    void* gc; /* Dummy (does not exist originally) */
} PyInterpreterState;
}  // namespace Python2

namespace Python3_5 {
typedef struct _is
{
    struct _is* next;
    struct _ts* tstate_head;
    PyObject* modules;
    PyObject* modules_by_index;
    PyObject* sysdict;
    PyObject* builtins;
    void* gc; /* Dummy (does not exist originally) */
} PyInterpreterState;
}  // namespace Python3_5

namespace Python3_7 {
typedef struct _is
{
    struct _is* next;
    struct _ts* tstate_head;

    int64_t id;
    int64_t id_refcount;
    PyThread_type_lock id_mutex;
    PyObject* modules;
    PyObject* modules_by_index;
    PyObject* sysdict;
    PyObject* builtins;
    void* gc; /* Dummy (does not exist originally) */
} PyInterpreterState;
}  // namespace Python3_7

namespace Python3_8 {
typedef struct _is
{
    struct _is* next;
    struct _ts* tstate_head;

    int64_t id;
    int64_t id_refcount;
    int requires_idref;
    PyThread_type_lock id_mutex;
    int finalizing;
    PyObject* modules;
    PyObject* modules_by_index;
    PyObject* sysdict;
    PyObject* builtins;
    void* gc; /* Dummy (does not exist originally) */
} PyInterpreterState;
}  // namespace Python3_8

namespace Python3_9 {
typedef struct _is
{
    struct _is* next;
    struct _is* tstate_head;

    /* Reference to the _PyRuntime global variable. This field exists
       to not have to pass runtime in addition to tstate to a function.
       Get runtime from tstate: tstate->interp->runtime. */
    struct pyruntimestate* runtime;

    int64_t id;
    int64_t id_refcount;
    int requires_idref;
    PyThread_type_lock id_mutex;

    int finalizing;

    struct _ceval_state ceval;
    struct Python3_8::_gc_runtime_state gc;
    PyObject* modules;
    PyObject* modules_by_index;
    PyObject* sysdict;
    PyObject* builtins;
} PyInterpreterState;
}  // namespace Python3_9

namespace Python3_11 {

struct _ceval_state
{
    int recursion_limit;
    _Py_atomic_int eval_breaker;
    _Py_atomic_int gil_drop_request;
    struct _pending_calls pending;
};

typedef struct _is
{
    struct _is* next;
    struct pythreads
    {
        uint64_t next_unique_id;
        struct _ts* head;
        long count;
        size_t stacksize;
    } threads;
    struct pyruntimestate* runtime;
    int64_t id;
    int64_t id_refcount;
    int requires_idref;
    PyThread_type_lock id_mutex;
    int _initialized;
    int finalizing;
    bool _static;
    struct _ceval_state ceval;
    struct Python3_8::_gc_runtime_state gc;
    PyObject* modules;
    PyObject* modules_by_index;
    PyObject* sysdict;
    PyObject* builtins;
} PyInterpreterState;
}  // namespace Python3_11

typedef union {
    Python2::PyInterpreterState v2;
    Python3_5::PyInterpreterState v3_5;
    Python3_7::PyInterpreterState v3_7;
    Python3_8::PyInterpreterState v3_8;
    Python3_9::PyInterpreterState v3_9;
    Python3_11::PyInterpreterState v3_11;
} PyInterpreterState;
}  // namespace pystack
