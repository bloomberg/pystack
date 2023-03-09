#pragma once

#include "memory"
#include "unordered_map"

#include "mem.h"
#include "process.h"
#include "pycode.h"

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

    // Methods
    void resolveLocalVariables();

  private:
    // Data members
    const std::shared_ptr<const AbstractProcessManager> d_manager{};
    remote_addr_t d_addr{};
    ssize_t d_frame_no{};
    remote_addr_t d_prev_addr{};
    std::shared_ptr<FrameObject> d_prev{nullptr};
    FrameObject* d_next{nullptr};
    std::shared_ptr<CodeObject> d_code{nullptr};
    std::unordered_map<std::string, std::string> d_arguments{};
    std::unordered_map<std::string, std::string> d_locals{};
    bool d_is_entry;
};
}  // namespace pystack
