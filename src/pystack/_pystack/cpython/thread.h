#pragma once

#include "frame.h"
#include "interpreter.h"
#include "object.h"

namespace pystack {

// Dummy struct _frame
struct frame;

typedef int (*Py_tracefunc)(PyObject*, struct frame*, int, PyObject*);

namespace Python2 {
typedef struct _pythreadstate
{
    struct _pythreadstate* next;
    PyInterpreterState* interp;

    struct _frame* frame;
    int recursion_depth;
    char overflowed;
    char recursion_critical;
    int tracing;
    int use_tracing;

    Py_tracefunc c_profilefunc;
    Py_tracefunc c_tracefunc;
    PyObject* c_profileobj;
    PyObject* c_traceobj;

    PyObject* curexc_type;
    PyObject* curexc_value;
    PyObject* curexc_traceback;

    PyObject* exc_type;
    PyObject* exc_value;
    PyObject* exc_traceback;

    PyObject* dict;

    int tick_counter;

    int gilstate_counter;

    PyObject* async_exc;
    long thread_id;
} PyThreadState;
}  // namespace Python2

namespace Python3_4 {
typedef struct _pythreadstate
{
    struct _pythreadstate* prev;
    struct _pythreadstate* next;
    Python2::PyInterpreterState* interp;

    struct _frame* frame;
    int recursion_depth;
    char overflowed;
    char recursion_critical;
    int tracing;
    int use_tracing;

    Py_tracefunc c_profilefunc;
    Py_tracefunc c_tracefunc;
    PyObject* c_profileobj;
    PyObject* c_traceobj;

    PyObject* curexc_type;
    PyObject* curexc_value;
    PyObject* curexc_traceback;

    PyObject* exc_type;
    PyObject* exc_value;
    PyObject* exc_traceback;

    PyObject* dict;

    int gilstate_counter;

    PyObject* async_exc;
    long thread_id;
} PyThreadState;
}  // namespace Python3_4

namespace Python3_7 {
typedef struct _err_stackitem
{
    PyObject *exc_type, *exc_value, *exc_traceback;
    struct _err_stackitem* previous_item;
} _PyErr_StackItem;

typedef struct _pythreadstate
{
    struct _pythreadstate* prev;
    struct _pythreadstate* next;
    Python2::PyInterpreterState* interp;
    struct _frame* frame;
    int recursion_depth;
    char overflowed;
    char recursion_critical;
    int stackcheck_counter;
    int tracing;
    int use_tracing;
    Py_tracefunc c_profilefunc;
    Py_tracefunc c_tracefunc;
    PyObject* c_profileobj;
    PyObject* c_traceobj;
    PyObject* curexc_type;
    PyObject* curexc_value;
    PyObject* curexc_traceback;
    _PyErr_StackItem exc_state;
    _PyErr_StackItem* exc_info;
    PyObject* dict;
    int gilstate_counter;
    PyObject* async_exc;
    unsigned long thread_id;
} PyThreadState;
}  // namespace Python3_7

namespace Python3_11 {
typedef struct _err_stackitem
{
    PyObject *exc_type, *exc_value, *exc_traceback;
    struct _err_stackitem* previous_item;
} _PyErr_StackItem;

typedef struct _cframe
{
    int use_tracing;
    struct _interpreter_frame* current_frame;
    struct _cframe* previous;
} CFrame;

typedef struct _pythreadstate
{
    struct _pythreadstate* prev;
    struct _pythreadstate* next;
    PyInterpreterState* interp;
    int _initialized;
    int _static;
    int recursion_remaining;
    int recursion_limit;
    int recursion_headroom;
    int tracing;
    int tracing_what;
    CFrame* cframe;
    Py_tracefunc c_profilefunc;
    Py_tracefunc c_tracefunc;
    PyObject* c_profileobj;
    PyObject* c_traceobj;
    PyObject* curexc_type;
    PyObject* curexc_value;
    PyObject* curexc_traceback;
    _PyErr_StackItem* exc_info;
    PyObject* dict;
    int gilstate_counter;
    PyObject* async_exc;
    unsigned long thread_id;
    unsigned long native_thread_id;
} PyThreadState;
}  // namespace Python3_11

namespace Python3_12 {
typedef struct _err_stackitem
{
    PyObject* exc_value;
    struct _err_stackitem* previous_item;
} _PyErr_StackItem;

typedef struct _cframe
{
    struct _interpreter_frame* current_frame;
    struct _cframe* previous;
} CFrame;

struct _py_trashcan
{
    int delete_nesting;
    PyObject* delete_later;
};

typedef struct _pythreadstate
{
    struct _pythreadstate* prev;
    struct _pythreadstate* next;
    struct _is* interp;
    struct
    {
        unsigned int initialized : 1;
        unsigned int bound : 1;
        unsigned int unbound : 1;
        unsigned int bound_gilstate : 1;
        unsigned int active : 1;
        unsigned int finalizing : 1;
        unsigned int cleared : 1;
        unsigned int finalized : 1;
        unsigned int : 24;
    } _status;
    int py_recursion_remaining;
    int py_recursion_limit;
    int c_recursion_remaining;
    int recursion_headroom;
    int tracing;
    int what_event;
    CFrame* cframe;
    Py_tracefunc c_profilefunc;
    Py_tracefunc c_tracefunc;
    PyObject* c_profileobj;
    PyObject* c_traceobj;
    PyObject* current_exception;
    _PyErr_StackItem* exc_info;
    PyObject* dict;
    int gilstate_counter;
    PyObject* async_exc;
    unsigned long thread_id;
    unsigned long native_thread_id;
    struct _py_trashcan trash;
    void (*on_delete)(void*);
    void* on_delete_data;
    int coroutine_origin_tracking_depth;
    PyObject* async_gen_firstiter;
    PyObject* async_gen_finalizer;
    PyObject* context;
    uint64_t context_ver;
    uint64_t id;
    void* datastack_chunk;
    PyObject** datastack_top;
    PyObject** datastack_limit;
    _PyErr_StackItem exc_state;
    CFrame root_cframe;
} PyThreadState;
}  // namespace Python3_12

typedef union {
    Python2::PyThreadState v2;
    Python3_4::PyThreadState v3_4;
    Python3_7::PyThreadState v3_7;
    Python3_11::PyThreadState v3_11;
    Python3_12::PyThreadState v3_12;
} PyThreadState;

union CFrame {
    Python3_11::CFrame v3_11;
    Python3_12::CFrame v3_12;
};

}  // namespace pystack
