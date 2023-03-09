#pragma once

#include <cstdint>
#include <unistd.h>

namespace pystack {

// The following is the pthread structure and all
// type requirements taking from GLIB. Check the following
// for more information:
// https://github.com/bminor/glibc/blob/78fb88827362fbd2cc8aa32892ae5b015106e25c/nptl/descr.h#L130-L176

typedef struct
{
    int i[4];
} __128bits;

typedef struct
{
    void* tcb;
    void* dtv;
    void* self;
    int multiple_threads;
    int gscope_flag;
    uintptr_t sysinfo;
    uintptr_t stack_guard;
    uintptr_t pointer_guard;
    unsigned long unused_vgetcpu_cache[2];
    unsigned int feature_1;
    int __glibc_unused1;
    void* __private_tm[4];
    void* __private_ss;
    unsigned long long ssp_base;
    __128bits __glibc_unused2[8][4] __attribute__((aligned(32)));
    void* __padding[8];
} __tcbhead_t;

typedef struct
{
    union {
        struct
        {
            int multiple_threads;
            int gscope_flag;
        } header;
        void* __padding[24];
    };
    struct
    {
        struct list_head* next;
        struct list_head* prev;
    } list;
    pid_t tid;
} _pthread_structure_with_simple_header;

typedef struct
{
    union {
        __tcbhead_t header;
        void* __padding[24];
    };
    struct
    {
        struct list_head* next;
        struct list_head* prev;
    } list;
    pid_t tid;
} _pthread_structure_with_tcbhead;

}  // namespace pystack
