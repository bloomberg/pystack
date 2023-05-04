from _pystack.elf_common cimport CoreFileAnalyzer
from _pystack.elf_common cimport ProcessAnalyzer
from _pystack.mem cimport MemoryMapInformation
from _pystack.mem cimport VirtualMap
from _pystack.mem cimport remote_addr_t
from libc.stdint cimport uintptr_t
from libcpp.memory cimport shared_ptr
from libcpp.string cimport string as cppstring
from libcpp.utility cimport pair
from libcpp.vector cimport vector


cdef extern from "process.h" namespace "pystack::AbstractProcessManager":
    cdef enum InterpreterStatus:
        RUNNING
        FINALIZED
        UNKNOWN

cdef extern from "process.h" namespace "pystack":
    cdef cppclass AbstractProcessManager:
        remote_addr_t scanBSS() except+
        remote_addr_t scanHeap() except+
        remote_addr_t scanAllAnonymousMaps() except+
        remote_addr_t findInterpreterStateFromSymbols() except+
        remote_addr_t findInterpreterStateFromElfData() except+
        ssize_t copyMemoryFromProcess(remote_addr_t addr, ssize_t size, void *destination) except+
        vector[int] Tids() except+
        InterpreterStatus isInterpreterActive() except+
        pair[int, int] findPythonVersion()
        void setPythonVersion(pair[int, int] version)

    cdef cppclass ProcessManager(AbstractProcessManager):
        ProcessManager(int pid, int blocking, shared_ptr[ProcessAnalyzer] analyzer, vector[VirtualMap] memory_maps, MemoryMapInformation map_info) except+

    cdef cppclass CoreFileProcessManager(AbstractProcessManager):
        CoreFileProcessManager(int pid, shared_ptr[CoreFileAnalyzer] analyzer, vector[VirtualMap] memory_maps, MemoryMapInformation map_info) except+
