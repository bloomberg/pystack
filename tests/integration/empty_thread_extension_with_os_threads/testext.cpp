#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <assert.h>
#include <pthread.h>
#include <unistd.h>

#pragma GCC push_options
#pragma GCC optimize ("O0")

void*
os_thread(void*)
{
    sleep(10000);
}

pthread_t start_os_thread()
{
    pthread_t thread;
    int ret = pthread_create(&thread, NULL, &os_thread, NULL);
    assert(0 == ret);

    return ret;
}

void cancel_os_thread(pthread_t tid)
{
    pthread_join(tid, NULL);
}

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
    pthread_t tid = start_os_thread();
    pthread_setname_np(thread, "thread_foo");

    Py_BEGIN_ALLOW_THREADS ret = pthread_join(thread, NULL);
    Py_END_ALLOW_THREADS

    assert(0 == ret);
    cancel_os_thread(tid);
    Py_RETURN_NONE;
}

#pragma GCC pop_options

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
