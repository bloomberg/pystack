#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <sys/uio.h>
#include <syscall.h>
#include <system_error>
#include <unistd.h>
#include <utility>

#include "corefile.h"
#include "logging.h"
#include "mem.h"

namespace pystack {

static ssize_t
_process_vm_readv(
        pid_t pid,
        const struct iovec* lvec,
        unsigned long liovcnt,
        const struct iovec* rvec,
        unsigned long riovcnt,
        unsigned long flags)
{
    return syscall(SYS_process_vm_readv, pid, lvec, liovcnt, rvec, riovcnt, flags);
}

static const std::string PERM_MESSAGE = "Operation not permitted";
static const size_t CACHE_CAPACITY = 5e+7;  // 50MB

VirtualMap::VirtualMap(
        uintptr_t start,
        uintptr_t end,
        unsigned long filesize,
        std::string flags,
        unsigned long offset,
        std::string device,
        unsigned long inode,
        std::string pathname)
: d_start(start)
, d_end(end)
, d_filesize(filesize)
, d_flags(std::move(flags))
, d_offset(offset)
, d_device(std::move(device))
, d_inode(inode)
, d_path(std::move(pathname))
{
}

bool
VirtualMap::containsAddr(remote_addr_t addr) const
{
    return d_start <= addr && addr < d_end;
}

uintptr_t
VirtualMap::Start() const
{
    return d_start;
}

uintptr_t
VirtualMap::End() const
{
    return d_end;
}

unsigned long
VirtualMap::FileSize() const
{
    return d_filesize;
}

const std::string&
VirtualMap::Flags() const
{
    return d_flags;
}

unsigned long
VirtualMap::Offset() const
{
    return d_offset;
}

const std::string&
VirtualMap::Device() const
{
    return d_device;
}

unsigned long
VirtualMap::Inode() const
{
    return d_inode;
}

const std::string&
VirtualMap::Path() const
{
    return d_path;
}

size_t
VirtualMap::Size() const
{
    return d_end - d_start;
}

MemoryMapInformation::MemoryMapInformation()
: d_main_map(std::nullopt)
, d_bss(std::nullopt)
, d_heap(std::nullopt)
{
}

const std::optional<VirtualMap>&
MemoryMapInformation::MainMap()
{
    return d_main_map;
}

const std::optional<VirtualMap>&
MemoryMapInformation::Bss()
{
    return d_bss;
}

const std::optional<VirtualMap>&
MemoryMapInformation::Heap()
{
    return d_heap;
}

void
MemoryMapInformation::setMainMap(const VirtualMap& main_map)
{
    d_main_map = main_map;
}

void
MemoryMapInformation::setBss(const VirtualMap& bss)
{
    d_bss = bss;
}

void
MemoryMapInformation::setHeap(const VirtualMap& heap)
{
    d_heap = heap;
}

LRUCache::LRUCache(size_t capacity)
: d_cache_capacity(capacity)
, d_size(0){};

void
LRUCache::put(uintptr_t key, std::vector<char>&& value)
{
    size_t value_size = value.size();

    if (!can_fit(value_size)) {
        return;
    }

    auto it = d_cache.find(key);

    if (it != d_cache.end()) {
        d_cache_list.erase(it->second.it);
        d_cache.erase(it);
    }

    while (d_size + value_size > d_cache_capacity) {
        d_cache.erase(d_cache_list.back().key);
        d_size -= d_cache_list.back().size;
        d_cache_list.pop_back();
    }

    d_cache_list.push_front(LRUCache::ListNode{key, value_size});
    d_cache[key] = LRUCache::CacheValue{std::move(value), d_cache_list.begin()};
    d_size += value_size;
}

const std::vector<char>&
LRUCache::get(uintptr_t key)
{
    auto it = d_cache.find(key);
    if (it == d_cache.end()) {
        throw std::range_error("There is no such key in the cache");
    } else {
        auto node_it = it->second.it;
        d_cache_list.splice(d_cache_list.begin(), d_cache_list, node_it);
        return it->second.data;
    }
}

bool
LRUCache::exists(uintptr_t key)
{
    return (d_cache.find(key) != d_cache.end());
}

bool
LRUCache::can_fit(size_t size)
{
    return d_cache_capacity >= size;
}

ProcessMemoryManager::ProcessMemoryManager(pid_t pid, const std::vector<VirtualMap>& vmaps)
: d_pid(pid)
, d_vmaps(vmaps)
, d_lru_cache(CACHE_CAPACITY)
{
}

ProcessMemoryManager::ProcessMemoryManager(pid_t pid)
: d_pid(pid)
, d_lru_cache(CACHE_CAPACITY)
{
}

ssize_t
ProcessMemoryManager::readChunk(remote_addr_t addr, size_t len, char* dst) const
{
    struct iovec local[1];
    struct iovec remote[1];
    ssize_t result = 0;
    ssize_t read = 0;

    do {
        local[0].iov_base = dst + result;
        local[0].iov_len = len - result;
        remote[0].iov_base = reinterpret_cast<uint8_t*>(addr) + result;
        remote[0].iov_len = len - result;

        read = _process_vm_readv(d_pid, local, 1, remote, 1, 0);
        if (read < 0) {
            if (errno == EFAULT) {
                throw InvalidRemoteAddress();
            } else if (errno == EPERM) {
                throw std::runtime_error(PERM_MESSAGE);
            }
            throw std::system_error(errno, std::generic_category());
        }

        result += read;
    } while ((size_t)read != local[0].iov_len);

    return result;
}

ssize_t
ProcessMemoryManager::copyMemoryFromProcess(remote_addr_t addr, size_t len, void* dst) const
{
    auto vmap = std::find_if(d_vmaps.begin(), d_vmaps.end(), [&](const auto& vmap) {
        return vmap.containsAddr(addr) && vmap.containsAddr(addr + len - 1);
    });

    if (vmap == d_vmaps.end() || !d_lru_cache.can_fit(vmap->Size())) {
        return readChunk(addr, len, reinterpret_cast<char*>(dst));
    }

    uintptr_t key = vmap->Start();
    size_t chunk_size = vmap->Size();
    remote_addr_t vmap_start_addr = vmap->Start();
    size_t offset_addr = addr - vmap_start_addr;

    if (!d_lru_cache.exists(key)) {
        std::vector<char> buf(chunk_size);
        readChunk(vmap_start_addr, chunk_size, buf.data());
        d_lru_cache.put(key, std::move(buf));
    }

    std::memcpy(dst, d_lru_cache.get(key).data() + offset_addr, len);

    return len;
}

bool
ProcessMemoryManager::isAddressValid(remote_addr_t addr, const VirtualMap& map) const
{
    if (addr == (uintptr_t) nullptr) {
        return false;
    }
    return map.Start() <= addr && addr < map.End();
}

CorefileRemoteMemoryManager::CorefileRemoteMemoryManager(
        std::shared_ptr<CoreFileAnalyzer> analyzer,
        std::vector<VirtualMap>& vmaps)
: d_analyzer(std::move(analyzer))
, d_vmaps(vmaps)
{
    CoreFileExtractor extractor{d_analyzer};
    d_shared_libs = extractor.ModuleInformation();

    const char* filename = d_analyzer->d_filename.c_str();
    int fd = open(filename, O_RDONLY);

    if (fd == -1) {
        LOG(ERROR) << "Failed to open a file " << filename;
        throw RemoteMemCopyError();
    }

    StatusCode ret = readCorefile(fd, filename);
    int close_ret = close(fd);

    if (close_ret == -1) {
        LOG(ERROR) << "Failed to close a file " << filename;
        throw RemoteMemCopyError();
    }

    if (ret == StatusCode::ERROR) {
        throw RemoteMemCopyError();
    }
}

CorefileRemoteMemoryManager::StatusCode
CorefileRemoteMemoryManager::readCorefile(int fd, const char* filename) noexcept
{
    struct stat fileInfo = {0};

    if (fstat(fd, &fileInfo) == -1) {
        LOG(ERROR) << "Failed to get a file size for a file " << filename;
        return StatusCode::ERROR;
    }

    if (fileInfo.st_size == 0) {
        LOG(ERROR) << "File " << filename << " is empty";
        return StatusCode::ERROR;
    }

    d_corefile_size = fileInfo.st_size;

    void* map = mmap(0, d_corefile_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        LOG(ERROR) << "Failed to mmap a file " << filename;
        return StatusCode::ERROR;
    }

    d_corefile_data = std::unique_ptr<char, std::function<void(char*)>>(
            reinterpret_cast<char*>(map),
            [this](auto addr) {
                if (munmap(addr, d_corefile_size) == -1) {
                    LOG(ERROR) << "Failed to un-mmap a file " << d_analyzer->d_filename.c_str();
                }
            });

    int madvise_result = madvise(d_corefile_data.get(), d_corefile_size, MADV_RANDOM);

    if (madvise_result == -1) {
        LOG(WARNING) << "Madvise for a file " << filename << " failed";
    }

    return StatusCode::SUCCESS;
}

ssize_t
CorefileRemoteMemoryManager::copyMemoryFromProcess(remote_addr_t addr, size_t size, void* destination)
        const
{
    off_t offset_in_file = 0;

    StatusCode ret = getMemoryLocationFromCore(addr, &offset_in_file);

    if (ret == StatusCode::SUCCESS) {
        if (static_cast<size_t>(offset_in_file) > d_corefile_size) {
            throw InvalidRemoteAddress();
        }
        memcpy(destination, d_corefile_data.get() + offset_in_file, size);
        return size;
    }

    // The memory may be in the data segment of some shared library
    const std::string* filename = nullptr;
    ret = getMemoryLocationFromElf(addr, &filename, &offset_in_file);

    if (ret == StatusCode::ERROR) {
        throw InvalidRemoteAddress();
    }

    std::ifstream is(*filename, std::ifstream::binary);
    if (is) {
        is.seekg(offset_in_file);
        is.read((char*)destination, size);
    } else {
        LOG(ERROR) << "Failed to read memory from file " << *filename;
        throw InvalidRemoteAddress();
    }
    return size;
}

CorefileRemoteMemoryManager::StatusCode
CorefileRemoteMemoryManager::getMemoryLocationFromCore(remote_addr_t addr, off_t* offset_in_file) const
{
    auto corefile_it = std::find_if(d_vmaps.cbegin(), d_vmaps.cend(), [&](auto& map) {
        return (map.Start() <= addr && addr <= map.End()) && (map.FileSize() != 0 && map.Offset() != 0);
    });
    if (corefile_it == d_vmaps.cend()) {
        return StatusCode::ERROR;
    }

    unsigned long base = corefile_it->Offset() - corefile_it->Start();
    *offset_in_file = base + addr;
    return StatusCode::SUCCESS;
}

CorefileRemoteMemoryManager::StatusCode
CorefileRemoteMemoryManager::getMemoryLocationFromElf(
        remote_addr_t addr,
        const std::string** filename,
        off_t* offset_in_file) const
{
    auto shared_libs_it = std::find_if(d_shared_libs.cbegin(), d_shared_libs.cend(), [&](auto& map) {
        return map.start <= addr && addr <= map.end;
    });
    if (shared_libs_it == d_shared_libs.cend()) {
        return StatusCode::ERROR;
    }
    *filename = &shared_libs_it->filename;
    *offset_in_file = addr - shared_libs_it->start;
    return StatusCode::SUCCESS;
}

bool
CorefileRemoteMemoryManager::isAddressValid(remote_addr_t addr, const VirtualMap& map) const
{
    if (addr == (uintptr_t) nullptr) {
        return false;
    }
    return map.Start() <= addr && addr < map.Start() + map.Size();
}
}  // namespace pystack
