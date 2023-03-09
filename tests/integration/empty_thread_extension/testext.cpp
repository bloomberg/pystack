#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <assert.h>
#include <pthread.h>
#include <unistd.h>

void*
sleepThread(void*)
{
    PyGILState_STATE gilstate = PyGILState_Ensure();
    sleep(1000);
    PyGILState_Release(gilstate);
    return NULL;
}

PyObject*
sleep10(PyObject*, PyObject*)
{
    pthread_t thread;
    int ret = pthread_create(&thread, NULL, &sleepThread, NULL);
    assert(0 == ret);

    Py_BEGIN_ALLOW_THREADS ret = pthread_join(thread, NULL);
    Py_END_ALLOW_THREADS

    assert(0 == ret);
    Py_RETURN_NONE;
}

static PyMethodDef methods[] = {
        {"sleep10", sleep10, METH_NOARGS, "Sleep for 10 seconds"},
        {NULL, NULL, 0, NULL},
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {PyModuleDef_HEAD_INIT, "testext", "", -1, methods};

PyMODINIT_FUNC
PyInit_testext(void)
{
    return PyModule_Create(&moduledef);
}
#else
PyMODINIT_FUNC
inittestext(void)
{
    Py_InitModule("testext", methods);
}
#endif

