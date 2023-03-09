#pragma once

#include <elf.h>
#include <elfutils/libdwfl.h>
#include <gelf.h>

#if __has_include(<filesystem>)
#    include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#    include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#    error "No std::filesystem support detected"
#endif

extern "C" {
typedef bool
Dwfl_Memory_Callback(
        Dwfl* dwfl,
        int segndx,
        void** buffer,
        size_t* buffer_available,
        GElf_Addr vaddr,
        size_t minread,
        void* arg);

struct r_debug_info_module
{
    struct r_debug_info_module* next;
    /* FD is -1 iff ELF is NULL.  */
    int fd;
    Elf* elf;
    GElf_Addr l_ld;
    /* START and END are both zero if not valid.  */
    GElf_Addr start, end;
    bool disk_file_has_build_id;
    char name[0];
};

struct r_debug_info
{
    struct r_debug_info_module* module;
};

extern bool
dwfl_elf_phdr_memory_callback(
        Dwfl* dwfl,
        int ndx,
        void** buffer,
        size_t* buffer_available,
        GElf_Addr vaddr,
        size_t minread,
        void* arg);

extern int
dwfl_link_map_report(
        Dwfl* dwfl,
        const void* auxv,
        size_t auxv_size,
        Dwfl_Memory_Callback* memory_callback,
        void* memory_callback_arg,
        struct r_debug_info* r_debug_info);
}
