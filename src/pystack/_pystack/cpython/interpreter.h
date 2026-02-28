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

namespace Python3_12 {

struct _pythreadstate; /* Forward */

struct _pending_calls
{
    int busy;
    PyThread_type_lock lock;
    _Py_atomic_int calls_to_do;
    int async_exc;
    struct _pending_call
    {
        int (*func)(void*);
        void* arg;
    } calls[32];
    int first;
    int last;
};

struct _ceval_state
{
    _Py_atomic_int eval_breaker;
    _Py_atomic_int gil_drop_request;
    int recursion_limit;
    struct _gil_runtime_state* gil;
    int own_gil;
    _Py_atomic_int gc_scheduled;
    struct _pending_calls pending;
};

struct _import_state
{
    PyObject* modules;
    PyObject* modules_by_index;
    PyObject* importlib;
    int override_frozen_modules;
    int override_multi_interp_extensions_check;
    int dlopenflags;
    PyObject* import_func;
    struct
    {
        PyThread_type_lock mutex;
        unsigned long thread;
        int level;
    } lock;
    struct
    {
        int import_level;
        int64_t accumulated;
        int header;
    } find_and_load;
};

typedef struct _is
{
    struct _is* next;
    int64_t id;
    int64_t id_refcount;
    int requires_idref;
    PyThread_type_lock id_mutex;
    int _initialized;
    int finalizing;
    uint64_t monitoring_version;
    uint64_t last_restart_version;
    struct pythreads
    {
        uint64_t next_unique_id;
        _pythreadstate* head;
        long count;
        size_t stacksize;
    } threads;
    struct pyruntimestate* runtime;
    uintptr_t _finalizing;
    struct Python3_8::_gc_runtime_state gc;
    // Dictionary of the sys module
    PyObject* sysdict;
    // Dictionary of the builtins module
    PyObject* builtins;
    struct _ceval_state ceval;
    struct _import_state imports;
} PyInterpreterState;
}  // namespace Python3_12

namespace Python3_13 {

struct _pythreadstate; /* Forward */
struct PyMutex
{
    uint8_t v;
};
typedef int (*_Py_pending_call_func)(void*);
struct _pending_call
{
    _Py_pending_call_func func;
    void* arg;
    int flags;
};
struct _pending_calls
{
    void* handling_thread;
    PyMutex mutex;
    int32_t npending;
    int32_t max;
    int32_t maxloop;
    struct _pending_call calls[300];
    int first;
    int next;
};

struct _ceval_state
{
    uintptr_t instrumentation_version;
    int recursion_limit;
    struct _gil_runtime_state* gil;
    int own_gil;
    struct _pending_calls pending;
};

struct _import_state
{
    PyObject* modules;
    PyObject* modules_by_index;
    PyObject* importlib;
    int override_frozen_modules;
    int override_multi_interp_extensions_check;
    int dlopenflags;
    PyObject* import_func;
    struct
    {
        PyThread_type_lock mutex;
        unsigned long thread;
        int level;
    } lock;
    struct
    {
        int import_level;
        int64_t accumulated;
        int header;
    } find_and_load;
};

typedef struct _is
{
    struct _ceval_state ceval;
    struct _is* next;

    int64_t id;
    int64_t id_refcount;
    int requires_idref;
    PyThread_type_lock id_mutex;
    long _whence;
    int _initialized;
    int _ready;
    int finalizing;
    uintptr_t last_restart_version;
    struct pythreads
    {
        uint64_t next_unique_id;
        _pythreadstate* head;
        _pythreadstate* main;
        Py_ssize_t count;
        size_t stacksize;
    } threads;
    struct pyruntimestate* runtime;
    void* _finalizing;
    unsigned long _finalizing_id;
    struct Python3_8::_gc_runtime_state gc;
    // Dictionary of the sys module
    PyObject* sysdict;
    // Dictionary of the builtins module
    PyObject* builtins;
    struct _import_state imports;
} PyInterpreterState;
}  // namespace Python3_13

namespace Python3_14 {

struct _pythreadstate;

typedef struct
{
    Python3_13::PyMutex mutex;
    unsigned long long thread;
    size_t level;
} _PyRecursiveMutex;

struct _import_state
{
    PyObject* modules;
    PyObject* modules_by_index;
    PyObject* importlib;
    int override_frozen_modules;
    int override_multi_interp_extensions_check;
    PyObject* import_func;
    _PyRecursiveMutex lock;
    /* diagnostic info in PyImport_ImportModuleLevelObject() */
    struct
    {
        int import_level;
        int64_t accumulated;
        int header;
    } find_and_load;
};

struct _gil_runtime_state
{
    unsigned long interval;
    struct _pythreadstate* last_holder;
    int locked;
    unsigned long switch_number;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
#ifdef FORCE_SWITCHING
    pthread_cond_t switch_cond;
    pthread_mutex_t switch_mutex;
#endif
};

typedef struct _is
{
    struct _ceval_state ceval;
    void* _malloced;
    struct _is* next;
    int64_t id;
    Py_ssize_t id_refcount;
    int requires_idref;
    long _whence;
    int _initialized;
    int _ready;
    int finalizing;
    uintptr_t last_restart_version;
    struct pythreads
    {
        uint64_t next_unique_id;
        struct _pythreadstate* head;
        struct _pythreadstate* preallocated;
        struct _pythreadstate* main;
        Py_ssize_t count;
        size_t stacksize;
    } threads;
    void* runtime;
    struct _pythreadstate* _finalizing;
    unsigned long _finalizing_id;
    struct _gc_runtime_state gc;
    PyObject* sysdict;
    PyObject* builtins;
    struct _import_state imports;
    struct _gil_runtime_state _gil;
} PyInterpreterState;

}  // namespace Python3_14

}  // namespace pystack
