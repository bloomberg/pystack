from _pystack.mem cimport remote_addr_t
from _pystack.process cimport AbstractProcessManager
from libc.stdint cimport int64_t
from libcpp.memory cimport shared_ptr


cdef extern from "interpreter.h" namespace "pystack":
    cdef cppclass  InterpreterUtils:
        @staticmethod
        remote_addr_t getNextInterpreter(shared_ptr[AbstractProcessManager] manager, remote_addr_t interpreter_addr) except +

        @staticmethod
        int64_t getInterpreterId(shared_ptr[AbstractProcessManager] manager, remote_addr_t interpreter_addr) except +
