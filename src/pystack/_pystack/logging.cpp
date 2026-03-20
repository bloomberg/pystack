#include <stdexcept>
#include <string>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "logging.h"

namespace pystack {

static PyObject* g_logger = nullptr;
static int LOGGER_INITIALIZED = false;

void
initializePythonLoggerInterface()
{
    if (LOGGER_INITIALIZED) {
        return;
    }

    // Import the logging module and get a logger
    PyObject* logging_module = PyImport_ImportModule("logging");
    if (!logging_module) {
        PyErr_Print();
        throw std::runtime_error("Failed to import logging module");
    }

    PyObject* getLogger = PyObject_GetAttrString(logging_module, "getLogger");
    if (!getLogger) {
        Py_DECREF(logging_module);
        PyErr_Print();
        throw std::runtime_error("Failed to get logging.getLogger");
    }

    // Get logger for pystack._pystack
    PyObject* logger_name = PyUnicode_FromString("pystack._pystack");
    g_logger = PyObject_CallFunctionObjArgs(getLogger, logger_name, NULL);
    Py_DECREF(logger_name);
    Py_DECREF(getLogger);
    Py_DECREF(logging_module);

    if (!g_logger) {
        PyErr_Print();
        throw std::runtime_error("Failed to create logger");
    }

    LOGGER_INITIALIZED = true;
}

void
logWithPython(const std::string& message, int level)
{
    if (!LOGGER_INITIALIZED || !g_logger) {
        return;
    }

    // Ensure we hold the GIL before calling any Python APIs,
    // since LOG may be used from contexts where the GIL is released.
    PyGILState_STATE gstate = PyGILState_Ensure();

    if (PyErr_Occurred()) {
        PyGILState_Release(gstate);
        return;
    }

    // Get the log method name based on level
    const char* method_name;
    switch (level) {
        case DEBUG:
            method_name = "debug";
            break;
        case INFO:
            method_name = "info";
            break;
        case WARNING:
            method_name = "warning";
            break;
        case ERROR:
            method_name = "error";
            break;
        case CRITICAL:
            method_name = "critical";
            break;
        default:
            method_name = "info";
            break;
    }

    // Call the log method
    PyObject* py_message = PyUnicode_FromString(message.c_str());
    if (!py_message) {
        PyErr_Clear();
        PyGILState_Release(gstate);
        return;
    }

    PyObject* result = PyObject_CallMethod(g_logger, method_name, "O", py_message);
    Py_DECREF(py_message);

    if (!result) {
        PyErr_Clear();
        PyGILState_Release(gstate);
        return;
    }
    Py_DECREF(result);

    PyGILState_Release(gstate);
}

}  // namespace pystack
