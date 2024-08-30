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
            0,
            offsetof(T, f_localsplus)};
}

template<class T>
constexpr py_frame_v
py_framev311()
{
    return {sizeof(T),
            offsetof(T, previous),
            offsetof(T, f_code),
            0,
            offsetof(T, prev_instr),
            offsetof(T, localsplus),
            offsetof(T, is_entry)};
}

template<class T>
constexpr py_frame_v
py_framev312()
{
    return {sizeof(T),
            offsetof(T, previous),
            offsetof(T, f_code),
            0,
            offsetof(T, prev_instr),
            offsetof(T, localsplus),
            0,
            offsetof(T, owner)};
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
            offsetof(T, thread_id),
            offsetof(T, native_thread_id)};
}

template<class T>
constexpr py_thread_v
py_threadv313()
{
    return {sizeof(T),
            offsetof(T, prev),
            offsetof(T, next),
            offsetof(T, interp),
            offsetof(T, frame),
            offsetof(T, thread_id),
            offsetof(T, native_thread_id)};
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
constexpr py_is_v
py_isv312()
{
    return {sizeof(T),
            offsetof(T, next),
            offsetof(T, threads.head),
            offsetof(T, gc),
            offsetof(T, imports.modules),
            offsetof(T, sysdict),
            offsetof(T, builtins),
            offsetof(T, ceval.gil)};
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
constexpr py_cframe_v
py_cframe()
{
    return {
            sizeof(T),
            offsetof(T, current_frame),
    };
}

template<class T>
constexpr py_gilruntimestate_v
py_gilruntimestate()
{
    return {
            sizeof(T),
            offsetof(T, locked),
            offsetof(T, last_holder),
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
constexpr py_runtime_v
py_runtimev312()
{
    return {
            sizeof(T),
            offsetof(T, finalizing),
            offsetof(T, interpreters.head),
    };
}

template<class T>
constexpr py_runtime_v
py_runtimev313()
{
    return {
            sizeof(T),
            offsetof(T, finalizing),
            offsetof(T, interpreters.head),
            {},
            {},
            offsetof(T, debug_offsets.cookie),
            offsetof(T, debug_offsets.version),
            offsetof(T, debug_offsets.free_threaded),
            offsetof(T, debug_offsets.runtime_state.size),
            offsetof(T, debug_offsets.runtime_state.finalizing),
            offsetof(T, debug_offsets.runtime_state.interpreters_head),
            offsetof(T, debug_offsets.interpreter_state.size),
            offsetof(T, debug_offsets.interpreter_state.id),
            offsetof(T, debug_offsets.interpreter_state.next),
            offsetof(T, debug_offsets.interpreter_state.threads_head),
            offsetof(T, debug_offsets.interpreter_state.gc),
            offsetof(T, debug_offsets.interpreter_state.imports_modules),
            offsetof(T, debug_offsets.interpreter_state.sysdict),
            offsetof(T, debug_offsets.interpreter_state.builtins),
            offsetof(T, debug_offsets.interpreter_state.ceval_gil),
            offsetof(T, debug_offsets.interpreter_state.gil_runtime_state),
            offsetof(T, debug_offsets.interpreter_state.gil_runtime_state_enabled),
            offsetof(T, debug_offsets.interpreter_state.gil_runtime_state_locked),
            offsetof(T, debug_offsets.interpreter_state.gil_runtime_state_holder),
            offsetof(T, debug_offsets.thread_state.size),
            offsetof(T, debug_offsets.thread_state.prev),
            offsetof(T, debug_offsets.thread_state.next),
            offsetof(T, debug_offsets.thread_state.interp),
            offsetof(T, debug_offsets.thread_state.current_frame),
            offsetof(T, debug_offsets.thread_state.thread_id),
            offsetof(T, debug_offsets.thread_state.native_thread_id),
            offsetof(T, debug_offsets.thread_state.datastack_chunk),
            offsetof(T, debug_offsets.thread_state.status),
            offsetof(T, debug_offsets.interpreter_frame.size),
            offsetof(T, debug_offsets.interpreter_frame.previous),
            offsetof(T, debug_offsets.interpreter_frame.executable),
            offsetof(T, debug_offsets.interpreter_frame.instr_ptr),
            offsetof(T, debug_offsets.interpreter_frame.localsplus),
            offsetof(T, debug_offsets.interpreter_frame.owner),
            offsetof(T, debug_offsets.code_object.size),
            offsetof(T, debug_offsets.code_object.filename),
            offsetof(T, debug_offsets.code_object.name),
            offsetof(T, debug_offsets.code_object.qualname),
            offsetof(T, debug_offsets.code_object.linetable),
            offsetof(T, debug_offsets.code_object.firstlineno),
            offsetof(T, debug_offsets.code_object.argcount),
            offsetof(T, debug_offsets.code_object.localsplusnames),
            offsetof(T, debug_offsets.code_object.localspluskinds),
            offsetof(T, debug_offsets.code_object.co_code_adaptive),
            offsetof(T, debug_offsets.pyobject.size),
            offsetof(T, debug_offsets.pyobject.ob_type),
            offsetof(T, debug_offsets.type_object.size),
            offsetof(T, debug_offsets.type_object.tp_name),
            offsetof(T, debug_offsets.type_object.tp_repr),
            offsetof(T, debug_offsets.type_object.tp_flags),
            offsetof(T, debug_offsets.tuple_object.size),
            offsetof(T, debug_offsets.tuple_object.ob_item),
            offsetof(T, debug_offsets.tuple_object.ob_size),
            offsetof(T, debug_offsets.list_object.size),
            offsetof(T, debug_offsets.list_object.ob_item),
            offsetof(T, debug_offsets.list_object.ob_size),
            offsetof(T, debug_offsets.dict_object.size),
            offsetof(T, debug_offsets.dict_object.ma_keys),
            offsetof(T, debug_offsets.dict_object.ma_values),
            offsetof(T, debug_offsets.float_object.size),
            offsetof(T, debug_offsets.float_object.ob_fval),
            offsetof(T, debug_offsets.long_object.size),
            offsetof(T, debug_offsets.long_object.lv_tag),
            offsetof(T, debug_offsets.long_object.ob_digit),
            offsetof(T, debug_offsets.bytes_object.size),
            offsetof(T, debug_offsets.bytes_object.ob_size),
            offsetof(T, debug_offsets.bytes_object.ob_sval),
            offsetof(T, debug_offsets.unicode_object.size),
            offsetof(T, debug_offsets.unicode_object.state),
            offsetof(T, debug_offsets.unicode_object.length),
            offsetof(T, debug_offsets.unicode_object.asciiobject_size),
            offsetof(T, debug_offsets.gc.size),
            offsetof(T, debug_offsets.gc.collecting),
    };
}

template<class T>
constexpr py_type_v
py_type()
{
    return {sizeof(T), offsetof(T, tp_name), offsetof(T, tp_repr), offsetof(T, tp_flags)};
}

template<class T>
constexpr py_object_v
py_object()
{
    return {
            sizeof(T),
            offsetof(T, ob_type),
    };
}

template<class T>
constexpr py_bytes_v
py_bytes()
{
    return {
            sizeof(T),
            offsetof(T, ob_base.ob_size),
            offsetof(T, ob_sval),
    };
}

template<class T>
constexpr py_unicode_v
py_unicode()
{
    return {
            sizeof(T),
            offsetof(T, _base._base.state),
            offsetof(T, _base._base.length),
            offsetof(T, _base) + sizeof(T::_base._base),
    };
}

template<class T>
constexpr py_tuple_v
py_tuple()
{
    return {
            sizeof(T),
            offsetof(T, ob_base.ob_size),
            offsetof(T, ob_item),
    };
}

template<class T>
constexpr py_list_v
py_list()
{
    return {
            sizeof(T),
            offsetof(T, ob_base.ob_size),
            offsetof(T, ob_item),
    };
}

template<class T>
constexpr py_dict_v
py_dict()
{
    return {
            sizeof(T),
            offsetof(T, ma_keys),
            offsetof(T, ma_values),
    };
}

template<class T>
constexpr py_dictkeys_v
py_dictkeys()
{
    return {
            sizeof(T),
            offsetof(T, dk_size),
            {},
            offsetof(T, dk_nentries),
            offsetof(T, dk_indices),
    };
}

template<>
constexpr py_dictkeys_v
py_dictkeys<Python3_11::PyDictKeysObject>()
{
    return {
            sizeof(Python3_11::PyDictKeysObject),
            offsetof(Python3_11::PyDictKeysObject, dk_log2_size),
            offsetof(Python3_11::PyDictKeysObject, dk_kind),
            offsetof(Python3_11::PyDictKeysObject, dk_nentries),
            offsetof(Python3_11::PyDictKeysObject, dk_indices),
    };
}

template<class T>
constexpr py_dictvalues_v
py_dictvalues()
{
    return {
            sizeof(T),
            offsetof(T, values),
    };
}

template<class T>
constexpr py_float_v
py_float()
{
    return {
            sizeof(T),
            offsetof(T, ob_fval),
    };
}

template<class T>
constexpr py_long_v
py_long()
{
    return {
            sizeof(T),
            offsetof(T, ob_base.ob_size),
            offsetof(T, ob_digit),
    };
}

// ---- Python 2 --------------------------------------------------------------

python_v python_v2 = {
        py_tuple<PyTupleObject>(),
        py_list<PyListObject>(),
        {},
        {},
        {},
        py_float<PyFloatObject>(),
        py_long<_PyLongObject>(),
        {},
        {},
        py_object<PyObject>(),
        py_type<Python2::PyTypeObject>(),
        py_code<Python2::PyCodeObject>(),
        py_frame<Python2::PyFrameObject>(),
        py_thead_h<Python2::PyThreadState>(),
        py_is<Python2::PyInterpreterState>(),
};

// ---- Python 3.3 ------------------------------------------------------------

python_v python_v3_3 = {
        py_tuple<PyTupleObject>(),
        py_list<PyListObject>(),
        py_dict<Python3::PyDictObject>(),
        py_dictkeys<Python3_3::PyDictKeysObject>(),
        py_dictvalues<Python3::PyDictValuesObject>(),
        py_float<PyFloatObject>(),
        py_long<_PyLongObject>(),
        py_bytes<Python3::PyBytesObject>(),
        py_unicode<Python3::PyUnicodeObject>(),
        py_object<PyObject>(),
        py_type<Python3_3::PyTypeObject>(),
        py_code<Python3_3::PyCodeObject>(),
        py_frame<Python2::PyFrameObject>(),
        py_thead_h<Python2::PyThreadState>(),
        py_is<Python2::PyInterpreterState>(),
};

// ---- Python 3.4 ------------------------------------------------------------

python_v python_v3_4 = {
        py_tuple<PyTupleObject>(),
        py_list<PyListObject>(),
        py_dict<Python3::PyDictObject>(),
        py_dictkeys<Python3_3::PyDictKeysObject>(),
        py_dictvalues<Python3::PyDictValuesObject>(),
        py_float<PyFloatObject>(),
        py_long<_PyLongObject>(),
        py_bytes<Python3::PyBytesObject>(),
        py_unicode<Python3::PyUnicodeObject>(),
        py_object<PyObject>(),
        py_type<Python3_3::PyTypeObject>(),
        py_code<Python3_3::PyCodeObject>(),
        py_frame<Python2::PyFrameObject>(),
        py_thread<Python3_4::PyThreadState>(),
        py_is<Python3_5::PyInterpreterState>(),
};

// ---- Python 3.6 ------------------------------------------------------------

python_v python_v3_6 = {
        py_tuple<PyTupleObject>(),
        py_list<PyListObject>(),
        py_dict<Python3::PyDictObject>(),
        py_dictkeys<Python3_3::PyDictKeysObject>(),
        py_dictvalues<Python3::PyDictValuesObject>(),
        py_float<PyFloatObject>(),
        py_long<_PyLongObject>(),
        py_bytes<Python3::PyBytesObject>(),
        py_unicode<Python3::PyUnicodeObject>(),
        py_object<PyObject>(),
        py_type<Python3_3::PyTypeObject>(),
        py_code<Python3_6::PyCodeObject>(),
        py_frame<Python2::PyFrameObject>(),
        py_thread<Python3_4::PyThreadState>(),
        py_is<Python3_5::PyInterpreterState>(),
};

// ---- Python 3.7 ------------------------------------------------------------

python_v python_v3_7 = {
        py_tuple<PyTupleObject>(),
        py_list<PyListObject>(),
        py_dict<Python3::PyDictObject>(),
        py_dictkeys<Python3_3::PyDictKeysObject>(),
        py_dictvalues<Python3::PyDictValuesObject>(),
        py_float<PyFloatObject>(),
        py_long<_PyLongObject>(),
        py_bytes<Python3::PyBytesObject>(),
        py_unicode<Python3::PyUnicodeObject>(),
        py_object<PyObject>(),
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
        py_tuple<PyTupleObject>(),
        py_list<PyListObject>(),
        py_dict<Python3::PyDictObject>(),
        py_dictkeys<Python3_3::PyDictKeysObject>(),
        py_dictvalues<Python3::PyDictValuesObject>(),
        py_float<PyFloatObject>(),
        py_long<_PyLongObject>(),
        py_bytes<Python3::PyBytesObject>(),
        py_unicode<Python3::PyUnicodeObject>(),
        py_object<PyObject>(),
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
        py_tuple<PyTupleObject>(),
        py_list<PyListObject>(),
        py_dict<Python3::PyDictObject>(),
        py_dictkeys<Python3_3::PyDictKeysObject>(),
        py_dictvalues<Python3::PyDictValuesObject>(),
        py_float<PyFloatObject>(),
        py_long<_PyLongObject>(),
        py_bytes<Python3::PyBytesObject>(),
        py_unicode<Python3::PyUnicodeObject>(),
        py_object<PyObject>(),
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
        py_tuple<PyTupleObject>(),
        py_list<PyListObject>(),
        py_dict<Python3::PyDictObject>(),
        py_dictkeys<Python3_3::PyDictKeysObject>(),
        py_dictvalues<Python3::PyDictValuesObject>(),
        py_float<PyFloatObject>(),
        py_long<_PyLongObject>(),
        py_bytes<Python3::PyBytesObject>(),
        py_unicode<Python3::PyUnicodeObject>(),
        py_object<PyObject>(),
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
        py_tuple<PyTupleObject>(),
        py_list<PyListObject>(),
        py_dict<Python3::PyDictObject>(),
        py_dictkeys<Python3_11::PyDictKeysObject>(),
        py_dictvalues<Python3::PyDictValuesObject>(),
        py_float<PyFloatObject>(),
        py_long<_PyLongObject>(),
        py_bytes<Python3::PyBytesObject>(),
        py_unicode<Python3::PyUnicodeObject>(),
        py_object<PyObject>(),
        py_type<Python3_8::PyTypeObject>(),
        py_codev311<Python3_11::PyCodeObject>(),
        py_framev311<Python3_11::PyFrameObject>(),
        py_threadv311<Python3_11::PyThreadState>(),
        py_isv311<Python3_11::PyInterpreterState>(),
        py_runtime<Python3_11::PyRuntimeState>(),
        py_gc<Python3_8::_gc_runtime_state>(),
        py_cframe<Python3_11::CFrame>(),
};

// ---- Python 3.12 ------------------------------------------------------------

python_v python_v3_12 = {
        py_tuple<PyTupleObject>(),
        py_list<PyListObject>(),
        py_dict<Python3::PyDictObject>(),
        py_dictkeys<Python3_11::PyDictKeysObject>(),
        py_dictvalues<Python3::PyDictValuesObject>(),
        py_float<PyFloatObject>(),
        py_long<_PyLongObject>(),
        py_bytes<Python3::PyBytesObject>(),
        py_unicode<Python3_12::PyUnicodeObject>(),
        py_object<PyObject>(),
        py_type<Python3_8::PyTypeObject>(),
        py_codev311<Python3_12::PyCodeObject>(),
        py_framev312<Python3_12::PyFrameObject>(),
        py_threadv311<Python3_12::PyThreadState>(),
        py_isv312<Python3_12::PyInterpreterState>(),
        py_runtimev312<Python3_12::PyRuntimeState>(),
        py_gc<Python3_8::_gc_runtime_state>(),
        py_cframe<Python3_12::CFrame>(),
        py_gilruntimestate<Python3_9::_gil_runtime_state>(),
};

// ---- Python 3.13 ------------------------------------------------------------

python_v python_v3_13 = {
        py_tuple<PyTupleObject>(),
        py_list<PyListObject>(),
        py_dict<Python3::PyDictObject>(),
        py_dictkeys<Python3_11::PyDictKeysObject>(),
        py_dictvalues<Python3_13::PyDictValuesObject>(),
        py_float<PyFloatObject>(),
        py_long<_PyLongObject>(),
        py_bytes<Python3::PyBytesObject>(),
        py_unicode<Python3_12::PyUnicodeObject>(),
        py_object<PyObject>(),
        py_type<Python3_8::PyTypeObject>(),
        py_codev311<Python3_13::PyCodeObject>(),
        py_framev312<Python3_12::PyFrameObject>(),
        py_threadv313<Python3_13::PyThreadState>(),
        py_isv312<Python3_13::PyInterpreterState>(),
        py_runtimev313<Python3_13::PyRuntimeState>(),
        py_gc<Python3_13::_gc_runtime_state>(),
        py_cframe<Python3_12::CFrame>(),
        py_gilruntimestate<Python3_9::_gil_runtime_state>(),
};

// -----------------------------------------------------------------------------

const python_v*
getCPythonOffsets(int major, int minor)
{
    switch (major) {
        // ---- Python 2 -------------------------------------------------------
        case 2:
            if (minor != 7) {
                warnAboutUnsuportedVersion(major, minor);
            }
            return &python_v2;
            break;

        // ---- Python 3 -------------------------------------------------------
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
                case 12:
                    return &python_v3_12;
                    break;
                default:
                    warnAboutUnsuportedVersion(major, minor);
                    // fallthrough to latest
                case 13:
                    return &python_v3_13;
                    break;
            }
            break;
        default:
            throw std::runtime_error("Invalid python version");
    }
}
}  // namespace pystack
