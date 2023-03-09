#pragma once

#include <string>

namespace pystack {
// The reason this is a struct is so Cython can easily generate
// automatic conversions without explicit code.
struct NativeFrame
{
    unsigned long address;
    std::string symbol;
    std::string path;
    int linenumber;
    int colnumber;
    std::string library;
};
}  // namespace pystack
