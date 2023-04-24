from libc.stdint cimport uintptr_t
from libcpp.string cimport string as cppstring
from libcpp.vector cimport vector


cdef extern from "mem.h" namespace "pystack":
    ctypedef uintptr_t remote_addr_t

    cdef cppclass AbstractRemoteMemoryManager:
        ssize_t copyMemoryFromProcess(remote_addr_t addr, size_t size, void *destination) except+

    cdef cppclass ProcessMemoryManager(AbstractRemoteMemoryManager):
        ProcessMemoryManager(int pid) except+
        ssize_t copyMemoryFromProcess(remote_addr_t addr, size_t size, void *destination) except+


    cdef cppclass BlockingProcessMemoryManager(ProcessMemoryManager):
        BlockingProcessMemoryManager(int pid, vector[int]d_tids) except+
        ssize_t copyMemoryFromProcess(remote_addr_t addr, size_t size, void *destination) except+


    struct SimpleVirtualMap:
        uintptr_t start
        uintptr_t end
        cppstring filename
        cppstring buildid

    cdef cppclass VirtualMap:
        VirtualMap()
        VirtualMap(uintptr_t start, uintptr_t end, unsigned long filesize,
                   cppstring flags, unsigned long offset, cppstring permissions,
                   unsigned long inode, cppstring pathname)

    cdef cppclass MemoryMapInformation:
        MemoryMapInformation()
        void setMainMap(const VirtualMap& bss)
        void setBss(const VirtualMap& bss)
        void setHeap(const VirtualMap& heap)
