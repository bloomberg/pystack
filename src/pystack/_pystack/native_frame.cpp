#include "native_frame.h"

#include <string>

namespace pystack {

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

}  // namespace pystack
