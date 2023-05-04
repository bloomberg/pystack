#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/wait.h>
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

ProcessMemoryManager::ProcessMemoryManager(pid_t pid)
: d_pid(pid){};

ssize_t
ProcessMemoryManager::copyMemoryFromProcess(remote_addr_t addr, size_t len, void* buf) const
{
    struct iovec local[1];
    struct iovec remote[1];

    local[0].iov_base = buf;
    local[0].iov_len = len;
    remote[0].iov_base = (void*)addr;
    remote[0].iov_len = len;

    ssize_t result = _process_vm_readv(d_pid, local, 1, remote, 1, 0);
    if (result < 0) {
        if (errno == EFAULT) {
            throw InvalidRemoteAddress();
        } else if (errno == EPERM) {
            throw std::runtime_error(PERM_MESSAGE);
        }
        throw std::system_error(errno, std::generic_category());
    }

    if ((size_t)result != len) {
        throw InvalidCopiedMemory();
    }
    return result;
}

bool
ProcessMemoryManager::isAddressValid(remote_addr_t addr, const VirtualMap& map) const
{
    if (addr == (uintptr_t) nullptr) {
        return false;
    }
    return map.Start() <= addr && addr < map.End();
}

BlockingProcessMemoryManager::BlockingProcessMemoryManager(pid_t pid, const std::vector<int>& tids)
: ProcessMemoryManager(pid)
, d_tids(tids)
{
    for (auto& tid : tids) {
        LOG(INFO) << "Trying to stop thread " << tid;
        long ret = ptrace(PTRACE_ATTACH, tid, nullptr, nullptr);
        if (ret < 0) {
            int error = errno;
            detachFromProcess();
            if (error == EPERM) {
                throw std::runtime_error(PERM_MESSAGE);
            }
            throw std::system_error(error, std::generic_category());
        }
        LOG(INFO) << "Waiting for thread " << tid << " to be stopped";
        ret = waitpid(tid, nullptr, WUNTRACED);
        if (ret < 0) {
            // In some old kernels is not possible to use WUNTRACED with
            // threads (only the main thread will return a non zero value).
            if (tid == pid || errno != ECHILD) {
                detachFromProcess();
            }
        }
        LOG(INFO) << "Process " << tid << " attached";
    }
}

void
BlockingProcessMemoryManager::detachFromProcess()
{
    for (auto& tid : d_tids) {
        LOG(INFO) << "Detaching from thread " << tid;
        ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    }
}

BlockingProcessMemoryManager::~BlockingProcessMemoryManager()
{
    detachFromProcess();
}

CorefileRemoteMemoryManager::CorefileRemoteMemoryManager(
        std::shared_ptr<CoreFileAnalyzer> analyzer,
        std::vector<VirtualMap>& vmaps)
: d_analyzer(std::move(analyzer))
, d_vmaps(vmaps)
{
    CoreFileExtractor extractor{d_analyzer};
    d_shared_libs = extractor.ModuleInformation();
}

ssize_t
CorefileRemoteMemoryManager::copyMemoryFromProcess(remote_addr_t addr, size_t size, void* destination)
        const
{
    const std::string* filename = nullptr;
    off_t offset_in_file = 0;

    StatusCode ret = getMemoryLocationFromCore(addr, &filename, &offset_in_file);

    if (ret == StatusCode::ERROR) {
        // The memory may be in the data segment of some shared library
        getMemoryLocationFromElf(addr, &filename, &offset_in_file);
    }

    if (filename == nullptr) {
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
CorefileRemoteMemoryManager::getMemoryLocationFromCore(
        remote_addr_t addr,
        const std::string** filename,
        off_t* offset_in_file) const
{
    auto corefile_it = std::find_if(d_vmaps.cbegin(), d_vmaps.cend(), [&](auto& map) {
        return (map.Start() <= addr && addr <= map.End()) && (map.FileSize() != 0 && map.Offset() != 0);
    });
    if (corefile_it == d_vmaps.cend()) {
        return StatusCode::ERROR;
    }
    unsigned long base = corefile_it->Offset() - corefile_it->Start();
    *offset_in_file = base + addr;
    *filename = &d_analyzer->d_filename;
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
