#pragma once

#include <memory>
#include <unordered_map>

#include "mem.h"
#include "process.h"
#include "pycode.h"
#include "structure.h"

namespace pystack {

class FrameObject
{
  public:
    // Constructors
    FrameObject(
            const std::shared_ptr<const AbstractProcessManager>& manager,
            remote_addr_t addr,
            ssize_t frame_no);

    // Getters
    ssize_t FrameNo() const;
    std::shared_ptr<FrameObject> PreviousFrame();
    std::shared_ptr<CodeObject> Code();
    const std::unordered_map<std::string, std::string>& Arguments() const;
    const std::unordered_map<std::string, std::string>& Locals() const;
    bool IsEntryFrame() const;
    bool IsShim() const;

    // Methods
    void resolveLocalVariables();

  private:
    // Methods
    static bool getIsShim(
            const std::shared_ptr<const AbstractProcessManager>& manager,
            Structure<py_frame_v>& frame);

    static std::unique_ptr<CodeObject>
    getCode(const std::shared_ptr<const AbstractProcessManager>& manager, Structure<py_frame_v>& frame);

    bool
    isEntry(const std::shared_ptr<const AbstractProcessManager>& manager, Structure<py_frame_v>& frame);

    // Data members
    const std::shared_ptr<const AbstractProcessManager> d_manager{};
    remote_addr_t d_addr{};
    ssize_t d_frame_no{};
    std::shared_ptr<FrameObject> d_prev{nullptr};
    std::shared_ptr<CodeObject> d_code{nullptr};
    std::unordered_map<std::string, std::string> d_arguments{};
    std::unordered_map<std::string, std::string> d_locals{};
    bool d_is_entry;
    bool d_is_shim;
};
}  // namespace pystack
