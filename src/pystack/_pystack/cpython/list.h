#pragma once

#include "object.h"

namespace pystack {

typedef struct
{
    PyObject_VAR_HEAD PyObject** ob_item;
    Py_ssize_t allocated;
} PyListObject;

}  // namespace pystack
