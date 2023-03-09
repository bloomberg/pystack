from _pystack.pycode cimport CodeObject
from libcpp cimport bool
from libcpp.memory cimport unique_ptr
from libcpp.string cimport string as cppstring
from libcpp.unordered_map cimport unordered_map


cdef extern from "pyframe.h" namespace "pystack":

    cdef cppclass FrameObject:
        ssize_t FrameNo()
        unique_ptr[FrameObject] PreviousFrame()
        unique_ptr[CodeObject] Code()
        unordered_map[cppstring, cppstring] Arguments()
        unordered_map[cppstring, cppstring] Locals()
        bool IsEntryFrame()

        void resolveLocalVariables() except+
