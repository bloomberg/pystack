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
    remote_addr_t py_code_addr = manager->getField(frame, &py_frame_v::o_code);

    LOG(DEBUG) << std::hex << std::showbase << "Attempting to construct code object from address "
               << py_code_addr;

    uintptr_t last_instruction;
    if (manager->majorVersion() > 3 || (manager->majorVersion() == 3 && manager->minorVersion() >= 11)) {
        last_instruction = manager->getField(frame, &py_frame_v::o_prev_instr);
    } else {
        last_instruction = manager->getField(frame, &py_frame_v::o_lasti);
    }
    d_code = std::make_unique<CodeObject>(manager, py_code_addr, last_instruction);

    d_addr = addr;
    d_prev_addr = manager->getField(frame, &py_frame_v::o_back);
    LOG(DEBUG) << std::hex << std::showbase << "Previous frame address: " << d_prev_addr;
    d_frame_no = frame_no;
    d_prev = nullptr;
    d_next = nullptr;

    d_is_shim = false;
    if (manager->majorVersion() > 3 || (manager->majorVersion() == 3 && manager->minorVersion() >= 12)) {
        constexpr int FRAME_OWNED_BY_CSTACK = 3;
        d_is_shim = manager->getField(frame, &py_frame_v::o_owner) == FRAME_OWNED_BY_CSTACK;
    }

    if (manager->majorVersion() > 3 || (manager->majorVersion() == 3 && manager->minorVersion() >= 11)) {
        d_is_entry = false;  // Entry frames are detected below
    } else {
        d_is_entry = true;  // All frames are entry frames
    }

    if (d_prev_addr && d_frame_no < FRAME_LIMIT) {
        LOG(DEBUG) << std::hex << std::showbase << "Attempting to obtain new frame # " << d_frame_no + 1
                   << " from address " << d_prev_addr;
        d_prev = std::make_unique<FrameObject>(manager, d_prev_addr, frame_no + 1);
        // Skip artificial shim frames inserted before entry frames in 3.12+
        while (d_prev && d_prev->d_is_shim) {
            d_is_entry = true;
            d_prev = d_prev->d_prev;
        }
        if (d_prev) {
            d_prev->d_next = this;
        }
    }

    if (manager->majorVersion() == 3 && manager->minorVersion() == 11) {
        this->d_is_entry = manager->getField(frame, &py_frame_v::o_is_entry);
    }
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
