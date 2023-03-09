from posix.types cimport pid_t

from _pystack.elf_common cimport CoreFileAnalyzer
from _pystack.mem cimport SimpleVirtualMap
from libc.stdint cimport uintptr_t
from libcpp.memory cimport shared_ptr
from libcpp.string cimport string as cppstring
from libcpp.vector cimport vector


cdef extern from "corefile.h" namespace "pystack":
    struct CoreCrashInfo:
        int si_signo
        int si_errno
        int si_code
        int sender_pid
        int sender_uid
        uintptr_t failed_addr

    struct CorePsInfo:
        char state
        char sname
        char zomb
        char nice
        unsigned long flag
        int uid
        int gid
        pid_t pid
        pid_t ppid
        pid_t pgrp
        pid_t sid
        char fname[16]
        char psargs[80]

    struct CoreVirtualMap:
        uintptr_t start
        uintptr_t end
        unsigned long filesize
        cppstring flags
        unsigned long offset
        cppstring device
        unsigned long inode
        cppstring path
        cppstring buildid

    cdef cppclass CoreFileExtractor:
        CoreFileExtractor(shared_ptr[CoreFileAnalyzer] analyzer) except+
        int Pid() except+
        vector[CoreVirtualMap] MemoryMaps() except+
        vector[SimpleVirtualMap] ModuleInformation() except+
        cppstring extractExecutable() except+
        CoreCrashInfo extractFailureInfo() except+
        CorePsInfo extractPSInfo() except+
        vector[cppstring] missingModules() except+
        vector[CoreVirtualMap] extractMappedFiles() except+
