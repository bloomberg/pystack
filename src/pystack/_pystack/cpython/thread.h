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

namespace Python3_13 {
typedef struct _err_stackitem
{
    PyObject* exc_value;
    struct _err_stackitem* previous_item;
} _PyErr_StackItem;
typedef struct _pythreadstate
{
    struct _pythreadstate* prev;
    struct _pythreadstate* next;
    struct _is* interp;
    uintptr_t eval_breaker;
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
    int _whence;
    int state;
    int py_recursion_remaining;
    int py_recursion_limit;
    int c_recursion_remaining;
    int recursion_headroom;
    int tracing;
    int what_event;
    void* frame;
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

    PyObject* delete_later;
    uintptr_t critical_section;
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
    PyObject* previous_executor;
    uint64_t dict_global_version;
} PyThreadState;
}  // namespace Python3_13

namespace Python3_14 {

typedef struct _remote_debugger_support
{
    int32_t debugger_pending_call;
    char debugger_script_path[512];
} _PyRemoteDebuggerSupport;

typedef struct _pythreadstate
{
    struct _pythreadstate* prev;
    struct _pythreadstate* next;
    PyInterpreterState* interp;
    uintptr_t eval_breaker;
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
    int holds_gil;
    int _whence;
    int state;
    int py_recursion_remaining;
    int py_recursion_limit;
    int recursion_headroom;
    int tracing;
    int what_event;
    struct _PyInterpreterFrame* current_frame;
    Py_tracefunc c_profilefunc;
    Py_tracefunc c_tracefunc;
    PyObject* c_profileobj;
    PyObject* c_traceobj;
    PyObject* current_exception;
    Python3_13::_PyErr_StackItem* exc_info;
    PyObject* dict;
    int gilstate_counter;
    PyObject* async_exc;
    unsigned long thread_id;
    unsigned long native_thread_id;
    PyObject* delete_later;
    uintptr_t critical_section;
    int coroutine_origin_tracking_depth;
    PyObject* async_gen_firstiter;
    PyObject* async_gen_finalizer;
    PyObject* context;
    uint64_t context_ver;
    uint64_t id;
    void* datastack_chunk;
    PyObject** datastack_top;
    PyObject** datastack_limit;
    Python3_13::_PyErr_StackItem exc_state;
    PyObject* current_executor;
    uint64_t dict_global_version;
    PyObject* threading_local_key;
    PyObject* threading_local_sentinel;
    _PyRemoteDebuggerSupport remote_debugger_support;
} PyThreadState;

}  // namespace Python3_14

}  // namespace pystack
