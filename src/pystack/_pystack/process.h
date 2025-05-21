#pragma once

#include <cstdio>

#include <memory>
#include <optional>
#include <stdexcept>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "elf_common.h"
#include "mem.h"
#include "native_frame.h"
#include "pycompat.h"
#include "unwinder.h"
#include "version.h"

namespace pystack {

template<typename OffsetsStruct>
class Structure;

struct InvalidRemoteObject : public InvalidCopiedMemory
{
    const char* what() const noexcept override
    {
        return "Object copied from remote process is inconsistent";
    }
};

class ProcessTracer
{
  public:
    // Constructors
    ProcessTracer(pid_t pid);
    ProcessTracer(const ProcessTracer&) = delete;
    ProcessTracer& operator=(const ProcessTracer&) = delete;

    // Destructors
    ~ProcessTracer();

    // Methods
    std::vector<int> getTids() const;

  private:
    // Data members
    std::unordered_set<int> d_tids;

    // Methods
    void detachFromProcess();
};

class AbstractProcessManager : public std::enable_shared_from_this<AbstractProcessManager>
{
  public:
    // Enums
    enum InterpreterStatus { RUNNING = 1, FINALIZED = 2, UNKNOWN = -1 };

    // Constructor
    AbstractProcessManager(
            pid_t pid,
            std::vector<VirtualMap>&& memory_maps,
            MemoryMapInformation&& map_info);

    // Getters
    pid_t Pid() const;
    virtual const std::vector<int>& Tids() const = 0;
    remote_addr_t getAddressFromCache(const std::string& symbol) const;
    void registerAddressInCache(const std::string& symbol, remote_addr_t address) const;

    // Methods
    std::vector<NativeFrame> unwindThread(pid_t tid) const;
    bool isAddressValid(remote_addr_t addr) const;
    remote_addr_t findInterpreterStateFromPointer(remote_addr_t pointer) const;
    remote_addr_t findInterpreterStateFromPyRuntime(remote_addr_t runtime_addr) const;
    remote_addr_t findInterpreterStateFromSymbols() const;
    remote_addr_t findInterpreterStateFromElfData() const;
    remote_addr_t findInterpreterStateFromDebugOffsets() const;
    remote_addr_t findSymbol(const std::string& symbol) const;
    ssize_t copyMemoryFromProcess(remote_addr_t addr, size_t size, void* destination) const;
    template<typename T>
    ssize_t copyObjectFromProcess(remote_addr_t addr, T* destination) const;
    std::string getBytesFromAddress(remote_addr_t addr) const;
    std::string getStringFromAddress(remote_addr_t addr) const;
    std::string getCStringFromAddress(remote_addr_t addr) const;
    remote_addr_t scanAllAnonymousMaps() const;
    remote_addr_t scanBSS() const;
    remote_addr_t scanHeap() const;
    InterpreterStatus isInterpreterActive() const;
    std::pair<int, int> findPythonVersion() const;

    void setPythonVersionFromDebugOffsets();
    void setPythonVersion(const std::pair<int, int>& version);
    bool versionIsAtLeast(int required_major, int required_minor) const;
    bool isFreeThreaded() const;
    const python_v& offsets() const;

  protected:
    // Data members
    pid_t d_pid;
    std::optional<VirtualMap> d_main_map{std::nullopt};
    std::optional<VirtualMap> d_bss{std::nullopt};
    std::optional<VirtualMap> d_heap{std::nullopt};
    std::vector<VirtualMap> d_memory_maps;
    std::unique_ptr<AbstractRemoteMemoryManager> d_manager;
    std::unique_ptr<AbstractUnwinder> d_unwinder;
    mutable std::unordered_map<std::string, remote_addr_t> d_symbol_cache;
    std::shared_ptr<Analyzer> d_analyzer;
    int d_major{};
    int d_minor{};
    const python_v* d_py_v{};
    bool d_is_free_threaded;
    remote_addr_t d_debug_offsets_addr{};
    std::unique_ptr<python_v> d_debug_offsets{};
    mutable std::unordered_map<std::string, remote_addr_t> d_type_cache;

    // Methods
    bool isValidInterpreterState(remote_addr_t addr) const;
    bool isValidDictionaryObject(remote_addr_t addr) const;

  private:
    void warnIfOffsetsAreMismatched(remote_addr_t addr) const;
    remote_addr_t findPyRuntimeFromElfData() const;
    remote_addr_t findDebugOffsetsFromMaps() const;

    std::unique_ptr<python_v> loadDebugOffsets(Structure<py_runtime_v>& py_runtime) const;
    bool copyDebugOffsets(Structure<py_runtime_v>& py_runtime, python_v& debug_offsets) const;
    bool validateDebugOffsets(const Structure<py_runtime_v>& py_runtime, python_v& debug_offsets) const;
    void clampSizes(python_v& debug_offsets) const;
    remote_addr_t scanMemoryAreaForInterpreterState(const VirtualMap& map) const;
    remote_addr_t scanMemoryAreaForDebugOffsets(const VirtualMap& map) const;
};

template<typename T>
ssize_t
AbstractProcessManager::copyObjectFromProcess(remote_addr_t addr, T* destination) const
{
    return this->copyMemoryFromProcess(addr, sizeof(T), destination);
}

class ProcessManager : public AbstractProcessManager
{
  public:
    // Constructors
    ProcessManager(
            pid_t pid,
            const std::shared_ptr<ProcessTracer>& tracer,
            const std::shared_ptr<ProcessAnalyzer>& analyzer,
            std::vector<VirtualMap> memory_maps,
            MemoryMapInformation map_info);

    // Destructors
    virtual ~ProcessManager() = default;

    // Getters
    const std::vector<int>& Tids() const override;

  private:
    // Data members
    std::shared_ptr<ProcessTracer> tracer;
    std::vector<int> d_tids;
};

class CoreFileProcessManager : public AbstractProcessManager
{
  public:
    // Constructors
    CoreFileProcessManager(
            pid_t pid,
            const std::shared_ptr<CoreFileAnalyzer>& analyzer,
            std::vector<VirtualMap> memory_maps,
            MemoryMapInformation map_info);

    // Destructors
    virtual ~CoreFileProcessManager() = default;

    // Getters
    const std::vector<int>& Tids() const override;

  private:
    // Data members
    std::vector<int> d_tids;
    std::optional<std::string> d_executable;
};
}  // namespace pystack
