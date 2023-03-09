from libcpp.string cimport string as cppstring


cdef extern from "native_frame.h" namespace "pystack":
    struct NativeFrame:
        unsigned long address
        cppstring symbol
        cppstring path
        int linenumber
        int colnumber
        cppstring library
