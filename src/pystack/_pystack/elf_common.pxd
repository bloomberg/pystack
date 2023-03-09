from libc.stdint cimport uintptr_t
from libcpp.string cimport string as cppstring


cdef extern from "elf_common.h" namespace "pystack":
    cdef cppclass ProcessAnalyzer:
        ProcessAnalyzer(int pid) except+

    cdef cppclass CoreFileAnalyzer:
        CoreFileAnalyzer(cppstring filename) except+
        CoreFileAnalyzer(cppstring filename, cppstring executable) except+
        CoreFileAnalyzer(cppstring filename, cppstring executable, cppstring lib_search_path) except+

    struct SectionInfo:
        cppstring name
        cppstring flags
        uintptr_t addr
        uintptr_t corrected_addr
        size_t offset
        size_t size

    int getSectionInfo(const cppstring& filename, const cppstring& section_name, SectionInfo* result) except+
