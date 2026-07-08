#pragma once

#include <string>
#include <utility>

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

bool
is_eval_frame(const std::string& symbol, std::pair<int, int> python_version);

}  // namespace pystack
