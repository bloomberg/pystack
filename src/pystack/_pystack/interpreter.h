#pragma once

#include <cstdint>
#include <memory>

#include "mem.h"
#include "process.h"

namespace pystack {

class InterpreterUtils
{
  public:
    // Static Methods
    static remote_addr_t getNextInterpreter(
            const std::shared_ptr<const AbstractProcessManager>& manager,
            remote_addr_t interpreter_addr);

    static int getInterpreterId(
            const std::shared_ptr<const AbstractProcessManager>& manager,
            remote_addr_t interpreter_addr);
};

}  // namespace pystack
