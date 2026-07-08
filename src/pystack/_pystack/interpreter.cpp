#include <memory>

#include "interpreter.h"
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
        // Interpreter ID was added in Python 3.7, so for earlier versions
        // we just return the address as a unique identifier.
        return static_cast<int64_t>(interpreter_addr);
    }

    Structure<py_is_v> is(manager, interpreter_addr);
    return is.getField(&py_is_v::o_id);
}

}  // namespace pystack
