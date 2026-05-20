#include "native_frame.h"

#include <algorithm>
#include <set>
#include <string>

namespace pystack {

static const std::set<std::string> SYMBOL_IGNORELIST = {
        "PyObject_Call",
        "call_function",
        "classmethoddescr_call",
        "cmpwrapper_call",
        "fast_function",
        "function_call",
        "instance_call",
        "instancemethod_call",
        "methoddescr_call",
        "proxy_call",
        "slot_tp_call",
        "type_call",
        "weakref_call",
        "wrap_call",
        "wrapper_call",
        "wrapperdescr_call",
        "do_call_core",
};

static bool
starts_with(const std::string& str, const std::string& prefix)
{
    return str.rfind(prefix, 0) == 0;
}

bool
is_eval_frame(const std::string& symbol, std::pair<int, int> python_version)
{
    if (python_version < std::make_pair(3, 6)) {
        return symbol.find("PyEval_EvalFrameEx") != std::string::npos;
    }
    if (symbol.find("_PyEval_EvalFrameDefault") != std::string::npos) {
        return true;
    }
    // Python 3.14 tail call interpreter uses LLVM-generated functions
    if (starts_with(symbol, "_TAIL_CALL_") && symbol.find(".llvm.") != std::string::npos) {
        return true;
    }
    // Python 3.15+ tail call interpreter drops the .llvm. suffix
    if (python_version >= std::make_pair(3, 15) and starts_with(symbol, "_TAIL_CALL_")) {
        return true;
    }
    return false;
}

NativeFrame::FrameType
frame_type(const NativeFrame& frame, std::optional<std::pair<int, int>> python_version)
{
    const std::string& symbol = frame.symbol;

    if (python_version && is_eval_frame(symbol, *python_version)) {
        return NativeFrame::FrameType::EVAL;
    }
    if (starts_with(symbol, "PyEval") || starts_with(symbol, "_PyEval")) {
        return NativeFrame::FrameType::IGNORE;
    }
    if (starts_with(symbol, "_Py")) {
        return NativeFrame::FrameType::IGNORE;
    }
    if (starts_with(symbol, "_TAIL_CALL_")) {
        return NativeFrame::FrameType::IGNORE;
    }
    if (python_version && *python_version >= std::make_pair(3, 8)) {
        std::string lower = symbol;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.find("vectorcall") != std::string::npos) {
            return NativeFrame::FrameType::IGNORE;
        }
    }
    for (const auto& ignored : SYMBOL_IGNORELIST) {
        if (starts_with(symbol, ignored)) {
            return NativeFrame::FrameType::IGNORE;
        }
    }

    return NativeFrame::FrameType::OTHER;
}

}  // namespace pystack
