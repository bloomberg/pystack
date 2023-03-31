#pragma once

#include <cstdio>

#include <memory>
#include <optional>
#include <stdexcept>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "elf_common.h"
#include "mem.h"
#include "native_frame.h"
#include "pycompat.h"
#include "unwinder.h"
#include "version.h"

namespace pystack {

struct InvalidRemoteObject : public InvalidCopiedMemory
{
    const char* what() const noexcept override
    {
        return "Object copied from remote process is inconsistent";
    }
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

    // Methods
    std::vector<NativeFrame> unwindThread(pid_t tid) const;
    bool isAddressValid(remote_addr_t addr) const;
    remote_addr_t findInterpreterStateFromPointer(remote_addr_t pointer) const;
    remote_addr_t findInterpreterStateFromPyRuntime(remote_addr_t runtime_addr) const;
    remote_addr_t findInterpreterStateFromSymbols() const;
    remote_addr_t findInterpreterStateFromElfData() const;
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

    void setPythonVersion(const std::pair<int, int>& version);
    int majorVersion() const;
    int minorVersion() const;
    const python_v& offsets() const;

    template<typename T, typename U, auto PMD0, auto PMD1>
    inline auto& versionedField(const U& py_obj) const
    {
        offset_t offset = d_py_v->*PMD0.*PMD1;
        return (*((T*)(((char*)&py_obj) + offset)));
    }

    template<typename T, auto PMD1>
    inline auto& versionedThreadField(const PyThreadState& py_thread) const
    {
        return versionedField<T, PyThreadState, &python_v::py_thread, PMD1>(py_thread);
    }

    template<typename T, auto PMD1>
    inline auto& versionedInterpreterStateField(const PyInterpreterState& py_is) const
    {
        return versionedField<T, PyInterpreterState, &python_v::py_is, PMD1>(py_is);
    }

    template<typename T, auto PMD1>
    inline auto& versionedGcStatesField(const GCRuntimeState& py_gc) const
    {
        return versionedField<T, GCRuntimeState, &python_v::py_gc, PMD1>(py_gc);
    }

    template<typename T, auto PMD1>
    inline auto& versionedFrameField(const PyFrameObject& py_frame) const
    {
        return versionedField<T, PyFrameObject, &python_v::py_frame, PMD1>(py_frame);
    }

    template<typename T, auto PMD1>
    inline auto& versionedCodeField(const PyCodeObject& py_code) const
    {
        return versionedField<T, PyCodeObject, &python_v::py_code, PMD1>(py_code);
    }

    template<auto PMD1>
    inline auto versionedCodeOffset() const
    {
        return d_py_v->py_code.*PMD1;
    }

    template<typename T, auto PMD1>
    inline auto& versionedRuntimeField(const PyRuntimeState& py_runtime) const
    {
        return versionedField<T, PyRuntimeState, &python_v::py_runtime, PMD1>(py_runtime);
    }

    template<typename T, auto PMD1>
    inline auto& versionedTypeField(const PyTypeObject& py_type) const
    {
        return versionedField<T, PyTypeObject, &python_v::py_type, PMD1>(py_type);
    }

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

    // Methods
    bool isValidInterpreterState(remote_addr_t addr) const;
    bool isValidDictionaryObject(remote_addr_t addr) const;

  private:
    remote_addr_t scanMemoryAreaForInterpreterState(const VirtualMap& map) const;
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
            bool blocking,
            const std::shared_ptr<ProcessAnalyzer>& analyzer,
            std::vector<VirtualMap> memory_maps,
            MemoryMapInformation map_info);

    // Getters
    const std::vector<int>& Tids() const override;

  private:
    // Data members
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

    // Getters
    const std::vector<int>& Tids() const override;

  private:
    // Data members
    std::vector<int> d_tids;
    std::optional<std::string> d_executable;
};
}  // namespace pystack
