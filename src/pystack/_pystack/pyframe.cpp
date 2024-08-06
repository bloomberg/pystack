#include <memory>
#include <sstream>

#include "logging.h"
#include "mem.h"
#include "process.h"
#include "pycode.h"
#include "pycompat.h"
#include "pyframe.h"
#include "pytypes.h"
#include "version.h"

static constexpr int FRAME_LIMIT = 4096;

namespace pystack {
FrameObject::FrameObject(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        remote_addr_t addr,
        ssize_t frame_no)
: d_manager(manager)
{
    PyFrameObject frame;
    LOG(DEBUG) << "Copying frame number " << frame_no;
    LOG(DEBUG) << std::hex << std::showbase << "Copying frame struct from address " << addr;

    manager->copyMemoryFromProcess(addr, manager->offsets().py_frame.size, &frame);

    d_addr = addr;
    d_frame_no = frame_no;
    d_is_shim = getIsShim(manager, frame);

    ssize_t next_frame_no = frame_no + 1;
    if (d_is_shim) {
        LOG(DEBUG) << "Skipping over a shim frame inserted by the interpreter";
        next_frame_no = frame_no;
    } else {
        d_code = getCode(manager, frame);
    }
    std::tie(d_prev, d_is_entry) = getPrevAndIsEntry(manager, frame, next_frame_no);
}

bool
FrameObject::getIsShim(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        const PyFrameObject& frame)
{
    if (manager->versionIsAtLeast(3, 12)) {
        constexpr int FRAME_OWNED_BY_CSTACK = 3;
        return manager->getField(frame, &py_frame_v::o_owner) == FRAME_OWNED_BY_CSTACK;
    }
    return false;  // Versions before 3.12 don't have shim frames.
}

std::unique_ptr<CodeObject>
FrameObject::getCode(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        const PyFrameObject& frame)
{
    remote_addr_t py_code_addr = manager->getField(frame, &py_frame_v::o_code);

    LOG(DEBUG) << std::hex << std::showbase << "Attempting to construct code object from address "
               << py_code_addr;

    uintptr_t last_instruction;
    if (manager->versionIsAtLeast(3, 11)) {
        last_instruction = manager->getField(frame, &py_frame_v::o_prev_instr);
    } else {
        last_instruction = manager->getField(frame, &py_frame_v::o_lasti);
    }
    return std::make_unique<CodeObject>(manager, py_code_addr, last_instruction);
}

std::pair<std::shared_ptr<FrameObject>, bool>
FrameObject::getPrevAndIsEntry(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        const PyFrameObject& frame,
        ssize_t next_frame_no)
{
    auto prev_addr = manager->getField(frame, &py_frame_v::o_back);
    LOG(DEBUG) << std::hex << std::showbase << "Previous frame address: " << prev_addr;

    std::shared_ptr<FrameObject> prev;
    if (prev_addr) {
        prev = std::make_shared<FrameObject>(manager, prev_addr, next_frame_no);
    }

    bool is_entry;
    if (manager->versionIsAtLeast(3, 12)) {
        // This is an entry frame if the previous frame was a shim.
        // The previous frame should also be skipped in that case.
        is_entry = prev && prev->d_is_shim;
        if (is_entry) {
            prev = prev->d_prev;
        }
    } else if (manager->versionIsAtLeast(3, 11)) {
        // This is an entry frame if it has an entry flag set.
        is_entry = manager->getField(frame, &py_frame_v::o_is_entry);
    } else {
        // This is an entry frame, as all frames prior to 3.11 were.
        is_entry = true;
    }

    return std::make_pair(prev, is_entry);
}

void
FrameObject::resolveLocalVariables()
{
    LOG(DEBUG) << "Resolving local variables from frame number " << d_frame_no;

    const size_t n_arguments = d_code->NArguments();
    const size_t n_locals = d_code->Varnames().size();
    const remote_addr_t locals_addr = d_addr + d_manager->getFieldOffset(&py_frame_v::o_localsplus);

    if (n_locals < n_arguments) {
        throw std::runtime_error("Found more arguments than local variables");
    }

    std::vector<remote_addr_t> tuple_buffer(n_locals);
    LOG(DEBUG) << "Copying buffer containing local variables";
    d_manager->copyMemoryFromProcess(locals_addr, n_locals * sizeof(remote_addr_t), tuple_buffer.data());

    auto addLocal = [&](size_t index, auto& map) {
        remote_addr_t addr = tuple_buffer[index];
        if (addr == (remote_addr_t) nullptr) {
            return;
        }

        std::string key = d_code->Varnames()[index];

        LOG(DEBUG) << "Copying local variable at address " << std::hex << std::showbase << addr;
        std::string value = Object(d_manager, addr).toString();
        LOG(DEBUG) << "Local variable resolved to: " << key << ": " << value;
        map.insert(std::make_pair(key, value));
    };

    LOG(DEBUG) << "Copying content of local variables";
    for (size_t i = 0; i < tuple_buffer.size(); ++i) {
        if (i < n_arguments) {
            addLocal(i, d_arguments);
        } else {
            addLocal(i, d_locals);
        }
    }
}

ssize_t
FrameObject::FrameNo() const
{
    return d_frame_no;
}

std::shared_ptr<FrameObject>
FrameObject::PreviousFrame()
{
    return d_prev;
}
std::shared_ptr<CodeObject>
FrameObject::Code()
{
    return d_code;
}

const std::unordered_map<std::string, std::string>&
FrameObject::Arguments() const
{
    return d_arguments;
}

const std::unordered_map<std::string, std::string>&
FrameObject::Locals() const
{
    return d_locals;
}

bool
FrameObject::IsEntryFrame() const
{
    return this->d_is_entry;
}

}  // namespace pystack
