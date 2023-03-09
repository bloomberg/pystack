#pragma once

#include "object.h"
#include <cstdint>

namespace pystack {

typedef uint32_t Py_UCS4;
typedef uint16_t Py_UCS2;
typedef uint8_t Py_UCS1;

typedef Py_UCS4 Py_UNICODE;
typedef Py_ssize_t Py_hash_t;

typedef struct
{
    PyObject_HEAD Py_ssize_t length;
    Py_hash_t hash;
    struct
    {
        unsigned int interned : 2;
        unsigned int kind : 3;
        unsigned int compact : 1;
        unsigned int ascii : 1;
        unsigned int ready : 1;
        unsigned int : 24;
    } state;
    wchar_t* wstr;
} PyASCIIObject;

typedef struct
{
    PyASCIIObject _base;
    Py_ssize_t utf8_length;
    char* utf8;
    Py_ssize_t wstr_length;
} PyCompactUnicodeObject;

typedef struct
{
    PyObject_VAR_HEAD Py_hash_t ob_shash;
    char ob_sval[1];
} PyBytesObject;

namespace Python2 {
typedef struct
{
    PyObject_HEAD Py_ssize_t length;
    Py_UNICODE* str;
    long hash;
    PyObject* defenc;
} PyUnicodeObject;

typedef struct
{
    PyObject_VAR_HEAD long ob_shash;
    int ob_sstate;
    char ob_sval[1];
} _PyStringObject;
}  // namespace Python2

namespace Python3 {
typedef struct
{
    PyCompactUnicodeObject _base;
    union {
        void* any;
        Py_UCS1* latin1;
        Py_UCS2* ucs2;
        Py_UCS4* ucs4;
    } data;
} PyUnicodeObject;
}  // namespace Python3

typedef union {
    Python2::PyUnicodeObject v2;
    Python3::PyUnicodeObject v3;
} PyUnicodeObject;

}  // namespace pystack
