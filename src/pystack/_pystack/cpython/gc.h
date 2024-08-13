#pragma once

#include "object.h"

namespace pystack {

constexpr int NUM_GENERATIONS = 3;

/* Running stats per generation */
struct gc_generation_stats
{
    Py_ssize_t collections;
    Py_ssize_t collected;
    Py_ssize_t uncollectable;
};

namespace Python3_7 {
typedef union _gc_head {
    struct
    {
        union _gc_head* gc_next;
        union _gc_head* gc_prev;
        Py_ssize_t gc_refs;
    } gc;
    long double dummy; /* force worst-case alignment */
} PyGC_Head;

struct gc_generation
{
    PyGC_Head head;
    int threshold; /* collection threshold */
    int count; /* count of allocations or collections of younger
          generations */
};

struct _gc_runtime_state
{
    PyObject* trash_delete_later;
    int trash_delete_nesting;
    int enabled;
    int debug;
    struct gc_generation generations[NUM_GENERATIONS];
    PyGC_Head* generation0;
    struct gc_generation permanent_generation;
    struct gc_generation_stats generation_stats[NUM_GENERATIONS];
    int collecting;
};

}  // namespace Python3_7

namespace Python3_8 {

typedef struct
{
    uintptr_t _gc_next;
    uintptr_t _gc_prev;
} PyGC_Head;

struct gc_generation
{
    PyGC_Head head;
    int threshold; /* collection threshold */
    int count; /* count of allocations or collections of younger
          generations */
};

struct _gc_runtime_state
{
    PyObject* trash_delete_later;
    int trash_delete_nesting;
    int enabled;
    int debug;
    struct gc_generation generations[NUM_GENERATIONS];
    PyGC_Head* generation0;
    struct gc_generation permanent_generation;
    struct gc_generation_stats generation_stats[NUM_GENERATIONS];
    int collecting;
    PyObject* garbage;
    PyObject* callbacks;
    Py_ssize_t long_lived_total;
    Py_ssize_t long_lived_pending;
};

}  // namespace Python3_8

namespace Python3_13 {

using Python3_8::gc_generation;

struct _gc_runtime_state
{
    PyObject* trash_delete_later;
    int trash_delete_nesting;
    int enabled;
    int debug;
    struct gc_generation young;
    struct gc_generation old[2];
    struct gc_generation permanent_generation;
    struct gc_generation_stats generation_stats[NUM_GENERATIONS];
    int collecting;
    PyObject* garbage;
    PyObject* callbacks;
    Py_ssize_t heap_size;
    Py_ssize_t work_to_do;
    int visited_space;
};

}  // namespace Python3_13
}  // namespace pystack
