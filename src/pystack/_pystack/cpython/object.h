#pragma once
#include <cstdint>
#include <sys/types.h>

static_assert(sizeof(void*) == sizeof(intptr_t));
#if INTPTR_MAX == INT64_MAX
#    define ENVIRONMENT64
#else
#    define ENVIRONMENT32
#endif

namespace pystack {
#define PyObject_HEAD PyObject ob_base;
#define PyObject_VAR_HEAD PyVarObject ob_base;

typedef ssize_t Py_ssize_t;

struct _typeobject;

typedef struct object
{
    ssize_t ob_refcnt;
    _typeobject* ob_type;
} PyObject;

typedef struct
{
    PyObject ob_base;
    Py_ssize_t ob_size; /* Number of items in variable part */
} PyVarObject;

typedef PyObject* (*ternaryfunc)(PyObject*, PyObject*, PyObject*);
typedef int (*inquiry)(PyObject*);
typedef void (*freefunc)(void*);
typedef void (*destructor)(PyObject*);
typedef int (*printfunc)(PyObject*, void*, int);
typedef PyObject* (*getattrfunc)(PyObject*, char*);
typedef PyObject* (*getattrofunc)(PyObject*, PyObject*);
typedef int (*setattrfunc)(PyObject*, char*, PyObject*);
typedef int (*setattrofunc)(PyObject*, PyObject*, PyObject*);
typedef int (*cmpfunc)(PyObject*, PyObject*);
typedef PyObject* (*reprfunc)(PyObject*);
typedef long (*hashfunc)(PyObject*);
typedef PyObject* (*richcmpfunc)(PyObject*, PyObject*, int);
typedef PyObject* (*getiterfunc)(PyObject*);
typedef PyObject* (*iternextfunc)(PyObject*);
typedef PyObject* (*descrgetfunc)(PyObject*, PyObject*, PyObject*);
typedef int (*descrsetfunc)(PyObject*, PyObject*, PyObject*);
typedef int (*initproc)(PyObject*, PyObject*, PyObject*);
typedef PyObject* (*newfunc)(_typeobject*, PyObject*, PyObject*);
typedef PyObject* (*allocfunc)(_typeobject*, Py_ssize_t);
typedef int (*visitproc)(PyObject*, void*);
typedef int (*traverseproc)(PyObject*, visitproc, void*);

namespace Python2 {
typedef struct _typeobject
{
    PyObject_VAR_HEAD const char* tp_name;
    Py_ssize_t tp_basicsize, tp_itemsize;
    destructor tp_dealloc;
    printfunc tp_print;
    getattrfunc tp_getattr;
    setattrfunc tp_setattr;
    cmpfunc tp_compare;
    reprfunc tp_repr;
    void* tp_as_number;
    void* tp_as_sequence;
    void* tp_as_mapping;
    hashfunc tp_hash;
    ternaryfunc tp_call;
    reprfunc tp_str;
    getattrofunc tp_getattro;
    setattrofunc tp_setattro;
    void* tp_as_buffer;
    long tp_flags;
    const char* tp_doc;
    traverseproc tp_traverse;
    inquiry tp_clear;
    richcmpfunc tp_richcompare;
    Py_ssize_t tp_weaklistoffset;
    getiterfunc tp_iter;
    iternextfunc tp_iternext;
    struct PyMethodDef* tp_methods;
    struct PyMemberDef* tp_members;
    struct PyGetSetDef* tp_getset;
    struct _typeobject* tp_base;
    PyObject* tp_dict;
    descrgetfunc tp_descr_get;
    descrsetfunc tp_descr_set;
    Py_ssize_t tp_dictoffset;
    initproc tp_init;
    allocfunc tp_alloc;
    newfunc tp_new;
    freefunc tp_free;
    inquiry tp_is_gc;
    PyObject* tp_bases;
    PyObject* tp_mro;
    PyObject* tp_cache;
    PyObject* tp_subclasses;
    PyObject* tp_weaklist;
    destructor tp_del;
    unsigned int tp_version_tag;
} PyTypeObject;
}  // namespace Python2

namespace Python3_3 {
typedef struct _typeobject
{
    PyObject_VAR_HEAD const char* tp_name; /* For printing, in format "<module>.<name>" */
    Py_ssize_t tp_basicsize, tp_itemsize; /* For allocation */
    destructor tp_dealloc;
    printfunc tp_print;
    getattrfunc tp_getattr;
    setattrfunc tp_setattr;
    void* tp_as_async;
    reprfunc tp_repr;
    void* tp_as_number;
    void* tp_as_sequence;
    void* tp_as_mapping;
    hashfunc tp_hash;
    ternaryfunc tp_call;
    reprfunc tp_str;
    getattrofunc tp_getattro;
    setattrofunc tp_setattro;
    void* tp_as_buffer;
    unsigned long tp_flags;
    const char* tp_doc;
    traverseproc tp_traverse;
    inquiry tp_clear;
    richcmpfunc tp_richcompare;
    Py_ssize_t tp_weaklistoffset;
    getiterfunc tp_iter;
    iternextfunc tp_iternext;
    struct PyMethodDef* tp_methods;
    struct PyMemberDef* tp_members;
    struct PyGetSetDef* tp_getset;
    struct _typeobject* tp_base;
    PyObject* tp_dict;
    descrgetfunc tp_descr_get;
    descrsetfunc tp_descr_set;
    Py_ssize_t tp_dictoffset;
    initproc tp_init;
    allocfunc tp_alloc;
    newfunc tp_new;
    freefunc tp_free;
    inquiry tp_is_gc;
    PyObject* tp_bases;
    PyObject* tp_mro;
    PyObject* tp_cache;
    PyObject* tp_subclasses;
    PyObject* tp_weaklist;
    destructor tp_del;
    unsigned int tp_version_tag;
} PyTypeObject;
}  // namespace Python3_3

namespace Python3_8 {
typedef struct _typeobject
{
    PyObject_VAR_HEAD const char* tp_name;
    Py_ssize_t tp_basicsize, tp_itemsize;
    destructor tp_dealloc;
    Py_ssize_t tp_vectorcall_offset;
    getattrfunc tp_getattr;
    setattrfunc tp_setattr;
    void* tp_as_async;
    reprfunc tp_repr;
    void* tp_as_number;
    void* tp_as_sequence;
    void* tp_as_mapping;
    hashfunc tp_hash;
    ternaryfunc tp_call;
    reprfunc tp_str;
    getattrofunc tp_getattro;
    setattrofunc tp_setattro;
    void* tp_as_buffer;
    unsigned long tp_flags;
    const char* tp_doc; /* Documentation string */
    traverseproc tp_traverse;
    inquiry tp_clear;
    richcmpfunc tp_richcompare;
    Py_ssize_t tp_weaklistoffset;
    getiterfunc tp_iter;
    iternextfunc tp_iternext;
    struct PyMethodDef* tp_methods;
    struct PyMemberDef* tp_members;
    struct PyGetSetDef* tp_getset;
    struct _typeobject* tp_base;
    PyObject* tp_dict;
    descrgetfunc tp_descr_get;
    descrsetfunc tp_descr_set;
    Py_ssize_t tp_dictoffset;
    initproc tp_init;
    allocfunc tp_alloc;
    newfunc tp_new;
    freefunc tp_free;
    inquiry tp_is_gc;
    PyObject* tp_bases;
    PyObject* tp_mro;
    PyObject* tp_cache;
    PyObject* tp_subclasses;
    PyObject* tp_weaklist;
    destructor tp_del;
    unsigned int tp_version_tag;
} PyTypeObject;
}  // namespace Python3_8

typedef union {
    Python2::PyTypeObject v2;
    Python3_3::PyTypeObject v3_3;
    Python3_8::PyTypeObject v3_8;
} PyTypeObject;

/* These flags are used to determine if a type is a subclass. */
constexpr long Pystack_TPFLAGS_INT_SUBCLASS = 1ul << 23u;
constexpr long Pystack_TPFLAGS_LONG_SUBCLASS = 1ul << 24u;
constexpr long Pystack_TPFLAGS_LIST_SUBCLASS = 1ul << 25u;
constexpr long Pystack_TPFLAGS_TUPLE_SUBCLASS = 1uL << 26u;
constexpr long Pystack_TPFLAGS_BYTES_SUBCLASS = 1uL << 27u;
constexpr long Pystack_TPFLAGS_UNICODE_SUBCLASS = 1uL << 28u;
constexpr long Pystack_TPFLAGS_DICT_SUBCLASS = 1uL << 29u;
constexpr long Pystack_TPFLAGS_BASE_EXC_SUBCLASS = 1uL << 30u;
constexpr long Pystack_TPFLAGS_TYPE_SUBCLASS = 1uL << 31u;

}  // namespace pystack
