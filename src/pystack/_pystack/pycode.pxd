from libcpp.string cimport string as cppstring
from libcpp.vector cimport vector


cdef extern from "pycode.h" namespace "pystack":
    cdef struct LocationInfo:
        int lineno
        int end_lineno
        int column
        int end_column

    cdef cppclass CodeObject:
        cppstring Filename()
        cppstring Scope()
        LocationInfo Location()
        int NArguments()
        const vector[cppstring] Varnames()
