#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

static int fifo_fd = -1;

void
writeAll(const char* buf, size_t len)
{
    while (len > 0) {
        ssize_t written = write(fifo_fd, buf, len);
        if (written <= 0) {
            if (written < 0 && errno == EINTR) {
                continue;
            }
            Py_FatalError(strerror(errno));
        }
        buf += written;
        len -= (size_t)written;
    }
}

// Announce that we're ready and then block forever.
extern "C" void
signalReadinessThenBlock(int)
{
    const char token[] = "ready";
    writeAll(token, sizeof(token) - 1);
    close(fifo_fd);  // Caller reads til EOF, so the FIFO must be closed.
    pause();
}

PyObject*
signal_readiness_then_block(PyObject*, PyObject* args)
{
    const char* path;
    if (!PyArg_ParseTuple(args, "s", &path)) {
        return NULL;
    }

    fifo_fd = open(path, O_WRONLY);
    if (fifo_fd < 0) {
        return PyErr_SetFromErrno(PyExc_OSError);
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &signalReadinessThenBlock;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGUSR1, &action, NULL) != 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    raise(SIGUSR1);
    Py_FatalError("the signal handler unexpectedly returned");
}

static PyMethodDef methods[] = {
        {"signal_readiness_then_block",
         signal_readiness_then_block,
         METH_VARARGS,
         "Write \"ready\" to a FIFO from a signal handler, then block forever"},
        {NULL, NULL, 0, NULL},
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {PyModuleDef_HEAD_INIT, "testext", "", -1, methods};

PyMODINIT_FUNC
PyInit_testext(void)
{
    PyObject* mod = PyModule_Create(&moduledef);
#    ifdef Py_GIL_DISABLED
    PyUnstable_Module_SetGIL(mod, Py_MOD_GIL_NOT_USED);
#    endif
    return mod;
}
#else
PyMODINIT_FUNC
inittestext(void)
{
    Py_InitModule("testext", methods);
}
#endif
