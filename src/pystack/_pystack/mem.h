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

#include <elf_common.h>

namespace pystack {
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

class MemoryMapInformation
{
  public:
    MemoryMapInformation();

    // Getters
    const std::optional<VirtualMap>& MainMap();
    const std::optional<VirtualMap>& Bss();
    const std::optional<VirtualMap>& Heap();

    // Setters

    void setMainMap(const VirtualMap& main_map);
    void setBss(const VirtualMap& bss);
    void setHeap(const VirtualMap& heap);

  private:
    // Data members
    std::optional<VirtualMap> d_main_map;
    std::optional<VirtualMap> d_bss;
    std::optional<VirtualMap> d_heap;
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

    // Methods
    ssize_t readChunk(remote_addr_t addr, size_t len, char* dst) const;
};

class BlockingProcessMemoryManager : public ProcessMemoryManager
{
  public:
    // Constructors
    explicit BlockingProcessMemoryManager(
            pid_t pid,
            const std::vector<int>& tids,
            const std::vector<VirtualMap>& vmaps);

    // Destructors
    ~BlockingProcessMemoryManager() override;

  private:
    // Data members
    std::vector<int> d_tids;

    // Methods
    void detachFromProcess();
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
};
}  // namespace pystack
