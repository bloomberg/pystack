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

typedef struct pyruntimestate
{
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

typedef union {
    Python3_7::PyRuntimeState v3_7;
    Python3_8::PyRuntimeState v3_8;
    Python3_9::PyRuntimeState v3_9;
    Python3_9::PyRuntimeState v3_11;
} PyRuntimeState;

}  // namespace pystack
