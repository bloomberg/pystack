#pragma once

#include "object.h"

namespace pystack {

typedef struct
{
    PyObject_HEAD double ob_fval;
} PyFloatObject;

}  // namespace pystack
