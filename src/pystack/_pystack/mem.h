#pragma once

#include <cstdint>
#include <fcntl.h>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <vector>

#include "elf_common.h"

namespace pystack {

using file_unique_ptr = std::unique_ptr<FILE, std::function<int(FILE*)>>;
typedef uintptr_t remote_addr_t;

struct RemoteMemCopyError : public std::exception
{
    const char* what() const noexcept override
    {
        return "Error occurred when copying memory from another process";
    }
};

struct InvalidRemoteAddress : public RemoteMemCopyError
{
    const char* what() const noexcept override
    {
        return "Invalid address in remote process";
    }
};

struct InvalidCopiedMemory : public RemoteMemCopyError
{
    const char* what() const noexcept override
    {
        return "Mismatched amount of memory received!";
    }
};

class VirtualMap
{
  public:
    // Constructors
    VirtualMap() = default;

    VirtualMap(
            uintptr_t start,
            uintptr_t end,
            unsigned long filesize,
            std::string flags,
            unsigned long offset,
            std::string dev,
            unsigned long inode,
            std::string pathname);

    // Getters
    uintptr_t Start() const;

    uintptr_t End() const;

    unsigned long FileSize() const;

    const std::string& Flags() const;

    unsigned long Offset() const;

    const std::string& Device() const;

    unsigned long Inode() const;

    const std::string& Path() const;

    size_t Size() const;

    // Methods
    bool containsAddr(remote_addr_t addr) const;

    // Permission helpers
    bool isExecutable() const
    {
        return d_flags.find('x') != std::string::npos;
    }

    bool isReadable() const
    {
        return d_flags.find('r') != std::string::npos;
    }

    bool isWritable() const
    {
        return d_flags.find('w') != std::string::npos;
    }

    bool isPrivate() const
    {
        return d_flags.find('p') != std::string::npos;
    }

  private:
    // Data members
    uintptr_t d_start{};
    uintptr_t d_end{};
    unsigned long d_filesize{};
    std::string d_flags{};
    unsigned long d_offset{};
    std::string d_device{};
    unsigned long d_inode{};
    std::string d_path{};
};

class LRUCache
{
  private:
    struct ListNode
    {
        uintptr_t key;
        size_t size;
    };

    struct CacheValue
    {
        std::vector<char> data;
        std::list<ListNode>::iterator it;
    };

  public:
    explicit LRUCache(size_t cache_capacity);

    void put(uintptr_t key, std::vector<char>&& value);
    const std::vector<char>& get(uintptr_t key);
    bool exists(uintptr_t key);
    bool can_fit(size_t size);

  private:
    std::list<ListNode> d_cache_list;
    std::unordered_map<uintptr_t, CacheValue> d_cache;
    size_t d_cache_capacity;
    size_t d_size;
};

class AbstractRemoteMemoryManager
{
  public:
    // Constructors
    explicit AbstractRemoteMemoryManager() = default;

    // Destructors
    virtual ~AbstractRemoteMemoryManager() = default;

    // Methods
    virtual ssize_t copyMemoryFromProcess(remote_addr_t addr, size_t size, void* destination) const = 0;
    virtual bool isAddressValid(remote_addr_t addr, const VirtualMap& map) const = 0;
};

class ProcessMemoryManager : public AbstractRemoteMemoryManager
{
    // Constructors
  public:
    explicit ProcessMemoryManager(pid_t pid);
    explicit ProcessMemoryManager(pid_t pid, const std::vector<VirtualMap>& vmaps);

    // Methods
    ssize_t copyMemoryFromProcess(remote_addr_t addr, size_t size, void* dst) const override;
    bool isAddressValid(remote_addr_t addr, const VirtualMap& map) const override;

  private:
    // Data members
    pid_t d_pid;
    std::vector<VirtualMap> d_vmaps;
    mutable LRUCache d_lru_cache;
    mutable file_unique_ptr d_memfile;

    // Methods
    ssize_t readChunk(remote_addr_t addr, size_t len, char* dst) const;
    ssize_t readChunkDirect(remote_addr_t addr, size_t len, char* dst) const;
    ssize_t readChunkThroughMemFile(remote_addr_t addr, size_t len, char* dst) const;
};

struct SimpleVirtualMap
{
    uintptr_t start;
    uintptr_t end;
    std::string filename;
    std::string buildid;
};

class CorefileRemoteMemoryManager : public AbstractRemoteMemoryManager
{
  public:
    // Constructors
    explicit CorefileRemoteMemoryManager(
            std::shared_ptr<CoreFileAnalyzer> analyzer,
            std::vector<VirtualMap>& vmaps);

    // Methods
    ssize_t copyMemoryFromProcess(remote_addr_t addr, size_t size, void* destination) const override;

    bool isAddressValid(remote_addr_t addr, const VirtualMap& map) const override;

  private:
    // Structs and Enums
    enum class StatusCode {
        SUCCESS,
        ERROR,
    };

    struct ElfLoadSegment
    {
        GElf_Addr vaddr;
        GElf_Off offset;
        GElf_Xword size;
    };
    // Cache for PT_LOAD segments
    mutable std::unordered_map<std::string, std::vector<ElfLoadSegment>> d_elf_load_segments_cache;

    // Data members
    std::shared_ptr<CoreFileAnalyzer> d_analyzer;
    std::vector<VirtualMap> d_vmaps;
    std::vector<SimpleVirtualMap> d_shared_libs;
    size_t d_corefile_size;
    std::unique_ptr<char, std::function<void(char*)>> d_corefile_data;

    StatusCode readCorefile(int fd, const char* filename) noexcept;
    StatusCode getMemoryLocationFromCore(remote_addr_t addr, off_t* offset_in_file) const;
    StatusCode getMemoryLocationFromElf(
            remote_addr_t addr,
            const std::string** filename,
            off_t* offset_in_file) const;
    StatusCode initLoadSegments(const std::string& filename) const;
};
}  // namespace pystack
