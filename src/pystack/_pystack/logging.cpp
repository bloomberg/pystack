#include <stdexcept>
#include <string>

#include "../_pystack_api.h"
#include "logging.h"

namespace pystack {

static int LOGGER_INITIALIZED = false;

void
initializePythonLoggerInterface()
{
    import_pystack___pystack();
    LOGGER_INITIALIZED = true;
}

void
logWithPython(const std::string& message, int level)
{
    if (!LOGGER_INITIALIZED) {
        throw std::runtime_error("Logger is not initialized");
    }
    if (!PyErr_Occurred()) {
        log_with_python(message.c_str(), level);
    }
}

}  // namespace pystack
