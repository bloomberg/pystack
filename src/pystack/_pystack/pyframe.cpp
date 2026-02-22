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

namespace pystack {
FrameObject::FrameObject(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        remote_addr_t addr,
        ssize_t frame_no)
: d_manager(manager)
{
    LOG(DEBUG) << "Copying frame number " << frame_no;
    LOG(DEBUG) << std::hex << std::showbase << "Copying frame struct from address " << addr;
    Structure<py_frame_v> frame(manager, addr);

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

    auto prev_addr = frame.getField(&py_frame_v::o_back);
    LOG(DEBUG) << std::hex << std::showbase << "Previous frame address: " << prev_addr;
    if (prev_addr) {
        d_prev = std::make_shared<FrameObject>(manager, prev_addr, next_frame_no);
    }
    d_is_entry = isEntry(manager, frame);
}

bool
FrameObject::getIsShim(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        Structure<py_frame_v>& frame)
{
    if (manager->versionIsAtLeast(3, 12)) {
        int owner = frame.getField(&py_frame_v::o_owner);
        if (manager->versionIsAtLeast(3, 14)) {
            return owner == Python3_14::FRAME_OWNED_BY_CSTACK
                   || owner == Python3_14::FRAME_OWNED_BY_INTERPRETER;
        }
        return owner == Python3_12::FRAME_OWNED_BY_CSTACK;
    }
    return false;  // Versions before 3.12 don't have shim frames.
}

std::unique_ptr<CodeObject>
FrameObject::getCode(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        Structure<py_frame_v>& frame)
{
    remote_addr_t py_code_addr = frame.getField(&py_frame_v::o_code);
    if (manager->versionIsAtLeast(3, 14)) {
        py_code_addr = py_code_addr & (~3);
    }

    if (py_code_addr == (remote_addr_t) nullptr) {
        // In Python 3.14+, the base/sentinel frame at the bottom of each
        // thread's frame stack has a NULL f_executable. This is an internal
        // interpreter frame that should be skipped.
        return nullptr;
    }

    LOG(DEBUG) << std::hex << std::showbase << "Attempting to construct code object from address "
               << py_code_addr;

    uintptr_t last_instruction;
    if (manager->versionIsAtLeast(3, 11)) {
        last_instruction = frame.getField(&py_frame_v::o_prev_instr);
    } else {
        last_instruction = frame.getField(&py_frame_v::o_lasti);
    }
    int32_t tlbc_index = -1;
    if (manager->versionIsAtLeast(3, 14) && manager->isFreeThreaded()) {
        uintptr_t tlbc_index_addr = frame.getFieldRemoteAddress(&py_frame_v::o_prev_instr)
                                    + sizeof(last_instruction) + sizeof(Python3_14::_PyStackRef);
        manager->copyMemoryFromProcess(tlbc_index_addr, sizeof(tlbc_index), &tlbc_index);
    }
    try {
        return std::make_unique<CodeObject>(manager, py_code_addr, last_instruction, tlbc_index);
    } catch (const RemoteMemCopyError& ex) {
        // This may not have been a code object at all, or it may have been
        // trashed by memory corruption. Either way, indicate that we failed
        // to understand what code this frame is running.
        return std::make_unique<CodeObject>("???", "???", LocationInfo{0, 0, 0, 0});
    }
}

bool
FrameObject::isEntry(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        Structure<py_frame_v>& frame)
{
    if (manager->versionIsAtLeast(3, 12)) {
        // This is an entry frame if the previous frame was a shim, or if
        // this is the most recent frame and is itself a shim (meaning that
        // the entry frame this shim was created for hasn't been pushed yet,
        // so the latest _PyEval_EvalFrameDefault call has no Python frames).
        return (d_prev && d_prev->d_is_shim) || (d_frame_no == 0 && d_is_shim);
    } else if (manager->versionIsAtLeast(3, 11)) {
        // This is an entry frame if it has an entry flag set.
        return frame.getField(&py_frame_v::o_is_entry);
    }
    return true;
}

void
FrameObject::resolveLocalVariables()
{
    LOG(DEBUG) << "Resolving local variables from frame number " << d_frame_no;
    if (d_code == nullptr) {
        LOG(INFO) << "Frame is a shim frame, skipping local variable resolution";
        return;
    }

    const size_t n_arguments = d_code->NArguments();
    const size_t n_locals = d_code->Varnames().size();
    Structure<py_frame_v> frame(d_manager, d_addr);
    const remote_addr_t locals_addr = frame.getFieldRemoteAddress(&py_frame_v::o_localsplus);

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

        if (d_manager->versionIsAtLeast(3, 14)) {
            // In Python 3.14, the local variable is a PyStackRef: a pointer
            // with extra flags set in its low bits. Ignore the flags.
            addr = addr & (~3);
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

bool
FrameObject::IsShim() const
{
    return this->d_is_shim;
}

}  // namespace pystack
