#pragma once

#include "object.h"

namespace pystack {

typedef struct
{
    PyObject_VAR_HEAD PyObject* ob_item[1];
} PyTupleObject;

}  // namespace pystack
