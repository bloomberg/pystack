#pragma once

#include <type_traits>

#include "object.h"

namespace pystack {

typedef struct
{
    PyObject_HEAD long ob_ival;
} _PyIntObject;

typedef std::conditional<sizeof(void*) >= 8, uint32_t, unsigned short>::type digit;

typedef struct
{
    PyObject_VAR_HEAD digit ob_digit[1];
} _PyLongObject;

}  // namespace pystack
