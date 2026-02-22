from _pystack.mem cimport remote_addr_t
from _pystack.process cimport AbstractProcessManager
from libcpp.memory cimport shared_ptr


cdef extern from "interpreter.h" namespace "pystack":
    cdef cppclass  InterpreterUtils:
        @staticmethod
        remote_addr_t getNextInterpreter(shared_ptr[AbstractProcessManager] manager, remote_addr_t interpreter_addr) except+

        @staticmethod
        int getInterpreterId(shared_ptr[AbstractProcessManager] manager, remote_addr_t interpreter_addr) except+
