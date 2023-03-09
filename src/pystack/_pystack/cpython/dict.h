#pragma once

#include "object.h"
#include "string.h"

namespace pystack {

namespace Python2 {
typedef struct
{
    /* Cached hash code of me_key.  Note that hash codes are C longs.
     * We have to use Py_ssize_t instead because dict_popitem() abuses
     * me_hash to hold a search finger.
     */
    Py_ssize_t me_hash;
    PyObject* me_key;
    PyObject* me_value;
} PyDictEntry;

typedef struct _dictobject
{
    PyObject_HEAD Py_ssize_t ma_fill;
    Py_ssize_t ma_used;
    Py_ssize_t ma_mask;
    PyDictEntry* ma_table;
} PyDictObject;

}  // namespace Python2

namespace Python3 {
typedef Py_ssize_t (*dict_lookup_func)(void* mp, PyObject* key, Py_hash_t hash, PyObject** value_addr);
struct PyDictKeysObject;

typedef struct
{
    Py_hash_t me_hash;
    PyObject* me_key;
    PyObject* me_value;
} PyDictKeyEntry;

/* See dictobject.c for actual layout of DictKeysObject */
typedef struct
{
    PyObject_HEAD Py_ssize_t ma_used;
    uint64_t ma_version_tag;
    PyDictKeysObject* ma_keys;
    PyObject** ma_values;
} PyDictObject;

}  // namespace Python3
namespace Python3_3 {
typedef struct _dictkeysobject
{
    Py_ssize_t dk_refcnt;
    Py_ssize_t dk_size;
    Python3::dict_lookup_func dk_lookup;
    Py_ssize_t dk_usable;
    Py_ssize_t dk_nentries;
    char dk_indices[]; /* char is required to avoid strict aliasing. */
} PyDictKeysObject;
}  // namespace Python3_3

namespace Python3_11 {

typedef struct
{
    PyObject* me_key;
    PyObject* me_value;
} PyDictUnicodeEntry;

typedef struct _dictkeysobject
{
    Py_ssize_t dk_refcnt;
    uint8_t dk_log2_size;
    uint8_t dk_log2_index_bytes;
    uint8_t dk_kind;
    uint32_t dk_version;
    Py_ssize_t dk_usable;
    Py_ssize_t dk_nentries;
    char dk_indices[]; /* char is required to avoid strict aliasing. */
} PyDictKeysObject;
}  // namespace Python3_11

}  // namespace pystack
