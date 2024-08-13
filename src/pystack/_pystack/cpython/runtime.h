#pragma once

#include "gc.h"
#include "interpreter.h"
#include "object.h"
#include "thread.h"

namespace pystack {

#ifndef FORCE_SWITCHING
#    define FORCE_SWITCHING 1
#endif

typedef void* PyThread_type_lock;

typedef struct _Py_atomic_address
{
    uintptr_t _value;
} _Py_atomic_address;

struct _Py_tss_t
{
    int _is_initialized;
    pthread_key_t _key;
};

namespace Python3_7 {

constexpr int NEXITFUNCS = 32;
typedef struct pyruntimestate
{
    int initialized;
    int core_initialized;
    PyThreadState* finalizing;

    struct pyinterpreters
    {
        PyThread_type_lock mutex;
        PyInterpreterState* head;
        PyInterpreterState* main;
        int64_t next_id;
    } interpreters;
    void (*exitfuncs[NEXITFUNCS])(void);
    int nexitfuncs;

    _gc_runtime_state gc;

    struct
    {
        _Py_atomic_address tstate_current;
    } gilstate;  // This is fake, but is are here for compatibility
} PyRuntimeState;
}  // namespace Python3_7

namespace Python3_8 {
constexpr int NEXITFUNCS = 32;

struct _pending_calls
{
    int finishing;
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

struct _gil_runtime_state
{
    unsigned long interval;
    _Py_atomic_address last_holder;
    _Py_atomic_int locked;
    unsigned long switch_number;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
#ifdef FORCE_SWITCHING
    pthread_cond_t switch_cond;
    pthread_mutex_t switch_mutex;
#endif
};

struct _gilstate_runtime_state
{
    int check_enabled;
    _Py_atomic_address tstate_current;
    void* getframe;
    PyInterpreterState* autoInterpreterState;
    _Py_tss_t autoTSSkey;
};

struct _ceval_runtime_state
{
    int recursion_limit;
    int tracing_possible;
    _Py_atomic_int eval_breaker;
    _Py_atomic_int gil_drop_request;
    struct _pending_calls pending;
    _Py_atomic_int signals_pending;
    struct _gil_runtime_state gil;
};

struct PyThreadState;

typedef struct pyruntimestate
{
    int preinitializing;
    int preinitialized;
    int core_initialized;
    int initialized;
    void* finalizing;

    struct pyinterpreters
    {
        PyThread_type_lock mutex;
        PyInterpreterState* head;
        PyInterpreterState* main;
        int64_t next_id;
    } interpreters;
    struct _xidregistry
    {
        PyThread_type_lock mutex;
        struct _xidregitem* head;
    } xidregistry;
    unsigned long main_thread;
    void (*exitfuncs[NEXITFUNCS])(void);
    int nexitfuncs;
    _gc_runtime_state gc;
    struct _ceval_runtime_state ceval;
    struct _gilstate_runtime_state gilstate;
} PyRuntimeState;
}  // namespace Python3_8

namespace Python3_9 {
constexpr int NEXITFUNCS = 32;

struct _gilstate_runtime_state
{
    int check_enabled;
    _Py_atomic_address tstate_current;
    PyInterpreterState* autoInterpreterState;
    _Py_tss_t autoTSSkey;
};

struct _gil_runtime_state
{
    unsigned long interval;
    _Py_atomic_address last_holder;
    _Py_atomic_int locked;
    unsigned long switch_number;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
#ifdef FORCE_SWITCHING
    pthread_cond_t switch_cond;
    pthread_mutex_t switch_mutex;
#endif
};

struct _ceval_runtime_state
{
    _Py_atomic_int signals_pending;
    struct _gil_runtime_state gil;
};

typedef struct pyruntimestate
{
    int preinitializing;
    int preinitialized;
    int core_initialized;
    int initialized;
    void* finalizing;

    struct pyinterpreters
    {
        PyThread_type_lock mutex;
        PyInterpreterState* head;
        PyInterpreterState* main;
        int64_t next_id;
    } interpreters;
    struct _xidregistry
    {
        PyThread_type_lock mutex;
        struct _xidregitem* head;
    } xidregistry;
    unsigned long main_thread;
    void (*exitfuncs[NEXITFUNCS])(void);
    int nexitfuncs;
    struct _ceval_runtime_state ceval;
    struct _gilstate_runtime_state gilstate;
    int gc;  // This is fake, but is here for compatibility
} PyRuntimeState;
}  // namespace Python3_9

namespace Python3_11 {
constexpr int NEXITFUNCS = 32;
typedef struct pyruntimestate
{
    int _initialized;
    int preinitializing;
    int preinitialized;
    int core_initialized;
    int initialized;
    PyThreadState* finalizing;

    struct pyinterpreters
    {
        PyThread_type_lock mutex;
        PyInterpreterState* head;
        PyInterpreterState* main;
        int64_t next_id;
    } interpreters;
    struct _xidregistry
    {
        PyThread_type_lock mutex;
        struct _xidregitem* head;
    } xidregistry;
    unsigned long main_thread;
    void (*exitfuncs[NEXITFUNCS])(void);
    int nexitfuncs;
    struct Python3_9::_ceval_runtime_state ceval;
    struct Python3_9::_gilstate_runtime_state gilstate;
    int gc;  // This is fake, but is here for compatibility
} PyRuntimeState;
}  // namespace Python3_11

namespace Python3_12 {
typedef struct pyruntimestate
{
    int _initialized;
    int preinitializing;
    int preinitialized;
    int core_initialized;
    int initialized;
    PyThreadState* finalizing;

    struct pyinterpreters
    {
        PyThread_type_lock mutex;
        PyInterpreterState* head;
        PyInterpreterState* main;
        int64_t next_id;
    } interpreters;
} PyRuntimeState;
}  // namespace Python3_12

namespace Python3_13 {

typedef struct _Py_DebugOffsets
{
    char cookie[8];
    uint64_t version;
    // Runtime state offset;
    struct _runtime_state
    {
        uint64_t size;
        uint64_t finalizing;
        uint64_t interpreters_head;
    } runtime_state;

    // Interpreter state offset;
    struct _interpreter_state
    {
        uint64_t size;
        uint64_t id;
        uint64_t next;
        uint64_t threads_head;
        uint64_t gc;
        uint64_t imports_modules;
        uint64_t sysdict;
        uint64_t builtins;
        uint64_t ceval_gil;
        uint64_t gil_runtime_state_locked;
        uint64_t gil_runtime_state_holder;
    } interpreter_state;

    // Thread state offset;
    struct _thread_state
    {
        uint64_t size;
        uint64_t prev;
        uint64_t next;
        uint64_t interp;
        uint64_t current_frame;
        uint64_t thread_id;
        uint64_t native_thread_id;
        uint64_t datastack_chunk;
        uint64_t status;
    } thread_state;

    // InterpreterFrame offset;
    struct _interpreter_frame
    {
        uint64_t size;
        uint64_t previous;
        uint64_t executable;
        uint64_t instr_ptr;
        uint64_t localsplus;
        uint64_t owner;
    } interpreter_frame;

    // Code object offset;
    struct _code_object
    {
        uint64_t size;
        uint64_t filename;
        uint64_t name;
        uint64_t qualname;
        uint64_t linetable;
        uint64_t firstlineno;
        uint64_t argcount;
        uint64_t localsplusnames;
        uint64_t localspluskinds;
        uint64_t co_code_adaptive;
    } code_object;

    // PyObject offset;
    struct _pyobject
    {
        uint64_t size;
        uint64_t ob_type;
    } pyobject;

    // PyTypeObject object offset;
    struct _type_object
    {
        uint64_t size;
        uint64_t tp_name;
    } type_object;

    // PyTuple object offset;
    struct _tuple_object
    {
        uint64_t size;
        uint64_t ob_item;
    } tuple_object;

    // Unicode object offset;
    struct _unicode_object
    {
        uint64_t size;
        uint64_t state;
        uint64_t length;
        size_t asciiobject_size;
    } unicode_object;

    // GC runtime state offset;
    struct _gc
    {
        uint64_t size;
        uint64_t collecting;
    } gc;
} _Py_DebugOffsets;

typedef struct pyruntimestate
{
    _Py_DebugOffsets debug_offsets;
    int _initialized;
    int preinitializing;
    int preinitialized;
    int core_initialized;
    int initialized;
    PyThreadState* finalizing;
    unsigned long _finalizing_id;

    struct pyinterpreters
    {
        PyMutex mutex;
        PyInterpreterState* head;
        PyInterpreterState* main;
        int64_t next_id;
    } interpreters;
} PyRuntimeState;

}  // namespace Python3_13
typedef union {
    Python3_7::PyRuntimeState v3_7;
    Python3_8::PyRuntimeState v3_8;
    Python3_9::PyRuntimeState v3_9;
    Python3_11::PyRuntimeState v3_11;
    Python3_12::PyRuntimeState v3_12;
    Python3_13::PyRuntimeState v3_13;
} PyRuntimeState;

}  // namespace pystack
