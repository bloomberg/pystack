#pragma once

#include <optional>
#include <string>
#include <utility>

namespace pystack {
// The reason this is a struct is so Cython can easily generate
// automatic conversions without explicit code.
struct NativeFrame
{
    enum class FrameType {
        IGNORE = 0,
        EVAL = 1,
        OTHER = 3,
    };

    unsigned long address;
    std::string symbol;
    std::string path;
    int linenumber;
    int colnumber;
    std::string library;
};

bool
is_eval_frame(const std::string& symbol, std::pair<int, int> python_version);

NativeFrame::FrameType
frame_type(const NativeFrame& frame, std::optional<std::pair<int, int>> python_version);

}  // namespace pystack
