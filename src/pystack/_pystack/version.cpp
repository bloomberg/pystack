#include <stdexcept>

#include "logging.h"
#include "version.h"

namespace pystack {

python_v* py_v;
int PYTHON_MAJOR_VERSION = 0;
int PYTHON_MINOR_VERSION = 0;

static void
warnAboutUnsuportedVersion(int major, int minor)
{
    LOG(WARNING) << "Unsupported Python version detected: " << major << "." << minor;
}

template<class T>
constexpr py_code_v
py_code()
{
    return {sizeof(T),
            offsetof(T, co_filename),
            offsetof(T, co_name),
            offsetof(T, co_lnotab),
            offsetof(T, co_firstlineno),
            offsetof(T, co_argcount),
            offsetof(T, co_varnames)};
}

template<class T>
constexpr py_code_v
py_codev311()
{
    return {sizeof(T),
            offsetof(T, co_filename),
            offsetof(T, co_name),
            offsetof(T, co_linetable),
            offsetof(T, co_firstlineno),
            offsetof(T, co_argcount),
            offsetof(T, co_localsplusnames),
            offsetof(T, co_code_adaptive)};
}

template<class T>
constexpr py_frame_v
py_frame()
{
    return {sizeof(T),
            offsetof(T, f_back),
            offsetof(T, f_code),
            offsetof(T, f_lasti),
            offsetof(T, f_localsplus)};
}

template<class T>
constexpr py_frame_v
py_framev311()
{
    return {sizeof(T),
            offsetof(T, previous),
            offsetof(T, f_code),
            offsetof(T, f_lasti),
            offsetof(T, localsplus),
            offsetof(T, is_entry)};
}

template<class T>
constexpr py_thread_v
py_thead_h()
{
    /* Hack. Python 3.3 and below don't have the prev field */
    return {sizeof(T),
            offsetof(T, next),
            offsetof(T, next),
            offsetof(T, interp),
            offsetof(T, frame),
            offsetof(T, thread_id)};
}

template<class T>
constexpr py_thread_v
py_thread()
{
    return {sizeof(T),
            offsetof(T, prev),
            offsetof(T, next),
            offsetof(T, interp),
            offsetof(T, frame),
            offsetof(T, thread_id)};
}

template<class T>
constexpr py_thread_v
py_threadv311()
{
    return {sizeof(T),
            offsetof(T, prev),
            offsetof(T, next),
            offsetof(T, interp),
            offsetof(T, cframe),
            offsetof(T, thread_id)};
}

template<class T>
constexpr py_is_v
py_is()
{
    return {sizeof(T),
            offsetof(T, next),
            offsetof(T, tstate_head),
            offsetof(T, gc),
            offsetof(T, modules),
            offsetof(T, sysdict),
            offsetof(T, builtins)};
}

template<class T>
constexpr py_is_v
py_isv311()
{
    return {sizeof(T),
            offsetof(T, next),
            offsetof(T, threads.head),
            offsetof(T, gc),
            offsetof(T, modules),
            offsetof(T, sysdict),
            offsetof(T, builtins)};
}

template<class T>
constexpr py_gc_v
py_gc()
{
    return {
            sizeof(T),
            offsetof(T, collecting),
    };
}

template<class T>
constexpr py_runtime_v
py_runtime()
{
    return {
            sizeof(T),
            offsetof(T, finalizing),
            offsetof(T, interpreters.head),
            offsetof(T, gc),
            offsetof(T, gilstate.tstate_current._value),
    };
}

template<class T>
constexpr py_type_v
py_type()
{
    return {sizeof(T), offsetof(T, tp_name), offsetof(T, tp_repr), offsetof(T, tp_flags)};
}

// ---- Python 2 --------------------------------------------------------------

python_v python_v2 = {
        py_type<Python2::PyTypeObject>(),
        py_code<Python2::PyCodeObject>(),
        py_frame<Python2::PyFrameObject>(),
        py_thead_h<Python2::PyThreadState>(),
        py_is<Python2::PyInterpreterState>(),
};

// ---- Python 3.3 ------------------------------------------------------------

python_v python_v3_3 = {
        py_type<Python3_3::PyTypeObject>(),
        py_code<Python3_3::PyCodeObject>(),
        py_frame<Python2::PyFrameObject>(),
        py_thead_h<Python2::PyThreadState>(),
        py_is<Python2::PyInterpreterState>(),
};

// ---- Python 3.4 ------------------------------------------------------------

python_v python_v3_4 = {
        py_type<Python3_3::PyTypeObject>(),
        py_code<Python3_3::PyCodeObject>(),
        py_frame<Python2::PyFrameObject>(),
        py_thread<Python3_4::PyThreadState>(),
        py_is<Python3_5::PyInterpreterState>(),
};

// ---- Python 3.6 ------------------------------------------------------------

python_v python_v3_6 = {
        py_type<Python3_3::PyTypeObject>(),
        py_code<Python3_6::PyCodeObject>(),
        py_frame<Python2::PyFrameObject>(),
        py_thread<Python3_4::PyThreadState>(),
        py_is<Python3_5::PyInterpreterState>(),
};

// ---- Python 3.7 ------------------------------------------------------------

python_v python_v3_7 = {
        py_type<Python3_3::PyTypeObject>(),
        py_code<Python3_6::PyCodeObject>(),
        py_frame<Python3_7::PyFrameObject>(),
        py_thread<Python3_7::PyThreadState>(),
        py_is<Python3_7::PyInterpreterState>(),
        py_runtime<Python3_7::PyRuntimeState>(),
        py_gc<Python3_7::_gc_runtime_state>(),
};

// ---- Python 3.8 ------------------------------------------------------------

python_v python_v3_8 = {
        py_type<Python3_8::PyTypeObject>(),
        py_code<Python3_8::PyCodeObject>(),
        py_frame<Python3_7::PyFrameObject>(),
        py_thread<Python3_7::PyThreadState>(),
        py_is<Python3_8::PyInterpreterState>(),
        py_runtime<Python3_8::PyRuntimeState>(),
        py_gc<Python3_8::_gc_runtime_state>(),
};

// ---- Python 3.9 ------------------------------------------------------------

python_v python_v3_9 = {
        py_type<Python3_8::PyTypeObject>(),
        py_code<Python3_8::PyCodeObject>(),
        py_frame<Python3_7::PyFrameObject>(),
        py_thread<Python3_7::PyThreadState>(),
        py_is<Python3_9::PyInterpreterState>(),
        py_runtime<Python3_9::PyRuntimeState>(),
        py_gc<Python3_8::_gc_runtime_state>(),
};

// ---- Python 3.10 ------------------------------------------------------------

python_v python_v3_10 = {
        py_type<Python3_8::PyTypeObject>(),
        py_code<Python3_8::PyCodeObject>(),
        py_frame<Python3_10::PyFrameObject>(),
        py_thread<Python3_7::PyThreadState>(),
        py_is<Python3_9::PyInterpreterState>(),
        py_runtime<Python3_9::PyRuntimeState>(),
        py_gc<Python3_8::_gc_runtime_state>(),
};

// ---- Python 3.11 ------------------------------------------------------------

python_v python_v3_11 = {
        py_type<Python3_8::PyTypeObject>(),
        py_codev311<Python3_11::PyCodeObject>(),
        py_framev311<Python3_11::PyFrameObject>(),
        py_threadv311<Python3_11::PyThreadState>(),
        py_isv311<Python3_11::PyInterpreterState>(),
        py_runtime<Python3_11::PyRuntimeState>(),
        py_gc<Python3_8::_gc_runtime_state>(),
};

// ----------------------------------------------------------------------------

const auto LATEST_VERSION = &python_v3_10;

const python_v*
getCPythonOffsets(int major, int minor)
{
    switch (major) {
        // ---- Python 2 ------------------------------------------------------------
        case 2:
            if (minor == 7) {
                return &python_v2;
            } else {
                warnAboutUnsuportedVersion(major, minor);
                return &python_v2;
            }
            break;

            // ---- Python 3
            // ------------------------------------------------------------
        case 3:
            switch (minor) {
                case 0:
                case 1:
                case 2:
                    warnAboutUnsuportedVersion(major, minor);
                    return &python_v3_3;
                    break;

                    // 3.3
                case 3:
                    return &python_v3_3;
                    break;

                    // 3.4, 3.5
                case 4:
                case 5:
                    return &python_v3_4;
                    break;

                    // 3.6
                case 6:
                    return &python_v3_6;
                    break;

                    // 3.7
                case 7:
                    return &python_v3_7;
                    break;

                    // 3.8
                case 8:
                    return &python_v3_8;
                    break;

                case 9:
                    return &python_v3_9;
                    break;

                case 10:
                    return &python_v3_10;
                    break;

                case 11:
                    return &python_v3_11;
                    break;

                default:
                    warnAboutUnsuportedVersion(major, minor);
                    return LATEST_VERSION;
            }
            break;
        default:
            throw std::runtime_error("Invalid python version");
    }
}
}  // namespace pystack
