#include <memory>

#include "interpreter.h"
#include "logging.h"
#include "process.h"
#include "structure.h"
#include "version.h"

namespace pystack {

remote_addr_t
InterpreterUtils::getNextInterpreter(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        remote_addr_t interpreter_addr)
{
    Structure<py_is_v> is(manager, interpreter_addr);
    return is.getField(&py_is_v::o_next);
}

int64_t
InterpreterUtils::getInterpreterId(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        remote_addr_t interpreter_addr)
{
    if (!manager->versionIsAtLeast(3, 7)) {
        // No support for subinterpreters so the only interpreter is ID 0.
        return 0;
    }

    Structure<py_is_v> is(manager, interpreter_addr);
    int64_t id_value = is.getField(&py_is_v::o_id);

    return id_value;
}

}  // namespace pystack
