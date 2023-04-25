#include <algorithm>
#include <cassert>
#include <memory>

#include "logging.h"
#include "mem.h"
#include "native_frame.h"
#include "process.h"
#include "pycompat.h"
#include "pyframe.h"
#include "pythread.h"
#include "version.h"

#include "cpython/pthread.h"

namespace pystack {

Thread::Thread(pid_t pid, pid_t tid)
: d_pid(pid)
, d_tid(tid)
{
}

pid_t
Thread::Tid() const
{
    return d_tid;
}

const std::vector<NativeFrame>&
Thread::NativeFrames() const
{
    return d_native_frames;
}

void
Thread::populateNativeStackTrace(const std::shared_ptr<const AbstractProcessManager>& manager)
{
    d_native_frames = manager->unwindThread(d_tid);
}

off_t tid_offset_in_pthread_struct = 0;

static off_t
findPthreadTidOffset(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        remote_addr_t interp_state_addr)
{
    LOG(DEBUG) << "Attempting to locate tid offset in pthread structure";
    PyInterpreterState is;
    manager->copyObjectFromProcess(interp_state_addr, &is);

    auto current_thread_addr =
            manager->versionedInterpreterStateField<remote_addr_t, &py_is_v::o_tstate_head>(is);

    auto thread_head = current_thread_addr;

    // Iterate over all Python threads until we find a thread that has a tid equal to
    // the process pid. This works because in the main thread the tid is equal to the pid,
    // so when this happens it has to happen on the main thread. Note that the main thread
    // is not necessarily at the head of the Python thread linked list

#if defined(__GLIBC__)
    // If we detect GLIBC, we can try the two main known structs for 'struct
    // pthread' that we know about to avoid having to do guess-work by doing a
    // linear scan over the struct.
    while (current_thread_addr != (remote_addr_t) nullptr) {
        PyThreadState current_thread;
        manager->copyObjectFromProcess(current_thread_addr, &current_thread);
        auto pthread_id_addr =
                manager->versionedThreadField<unsigned long, &py_thread_v::o_thread_id>(current_thread);

        pid_t the_tid;
        std::vector<off_t> glibc_pthread_offset_candidates = {
                offsetof(_pthread_structure_with_simple_header, tid),
                offsetof(_pthread_structure_with_tcbhead, tid)};
        for (off_t candidate : glibc_pthread_offset_candidates) {
            manager->copyObjectFromProcess((remote_addr_t)(pthread_id_addr + candidate), &the_tid);
            if (the_tid == manager->Pid()) {
                LOG(DEBUG) << "Tid offset located using GLIBC offsets at offset " << std::showbase
                           << std::hex << candidate << " in pthread structure";
                return candidate;
            }
        }
        remote_addr_t next_thread_addr =
                manager->versionedThreadField<remote_addr_t, &py_thread_v::o_next>(current_thread);
        if (next_thread_addr == current_thread_addr) {
            break;
        }
        current_thread_addr = next_thread_addr;
    }
#endif

    current_thread_addr = thread_head;

    while (current_thread_addr != (remote_addr_t) nullptr) {
        PyThreadState current_thread;
        manager->copyObjectFromProcess(current_thread_addr, &current_thread);
        auto pthread_id_addr =
                manager->versionedThreadField<unsigned long, &py_thread_v::o_thread_id>(current_thread);

        // Attempt to locate a field in the pthread struct that's equal to the pid.
        uintptr_t buffer[200];
        manager->copyObjectFromProcess(pthread_id_addr, &buffer);
        for (int i = 0; i < 200; i++) {
            if (static_cast<pid_t>(buffer[i]) == manager->Pid()) {
                off_t offset = sizeof(uintptr_t) * i;
                LOG(DEBUG) << "Tid offset located by scanning at offset " << std::showbase << std::hex
                           << offset << " in pthread structure";
                return offset;
            }
        }

        remote_addr_t next_thread_addr =
                manager->versionedThreadField<remote_addr_t, &py_thread_v::o_next>(current_thread);
        if (next_thread_addr == current_thread_addr) {
            break;
        }
        current_thread_addr = next_thread_addr;
    }
    LOG(ERROR) << "Could not find tid offset in pthread structure";
    return 0;
}

PyThread::PyThread(const std::shared_ptr<const AbstractProcessManager>& manager, remote_addr_t addr)
: Thread(0, 0)
{
    d_pid = manager->Pid();

    PyThreadState ts;
    LOG(DEBUG) << std::hex << std::showbase << "Copying main thread struct from address " << addr;
    manager->copyObjectFromProcess(addr, &ts);

    remote_addr_t frame_addr = getFrameAddr(manager, ts);
    if (frame_addr != (remote_addr_t) nullptr) {
        LOG(DEBUG) << std::hex << std::showbase << "Attempting to construct frame from address "
                   << frame_addr;
        d_first_frame = std::make_unique<FrameObject>(manager, frame_addr, 0);
    }

    d_addr = addr;
    remote_addr_t candidate_next_addr =
            manager->versionedThreadField<remote_addr_t, &py_thread_v::o_next>(ts);
    d_next_addr = candidate_next_addr == addr ? (remote_addr_t) nullptr : candidate_next_addr;

    d_pthread_id = manager->versionedThreadField<unsigned long, &py_thread_v::o_thread_id>(ts);
    d_tid = getThreadTid(manager, addr, d_pthread_id);
    d_next = nullptr;

    if (d_next_addr != (remote_addr_t)NULL) {
        LOG(DEBUG) << std::hex << std::showbase << "Attempting to construct a new thread address "
                   << d_next_addr;
        d_next = std::make_unique<PyThread>(manager, d_next_addr);
    }

    d_gil_status = calculateGilStatus(manager);
    d_gc_status = calculateGCStatus(ts, manager);
}

int
PyThread::getThreadTid(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        remote_addr_t thread_addr,
        unsigned long pthread_id)
{
    int the_tid = -1;
    if (manager->majorVersion() > 3 || (manager->majorVersion() == 3 && manager->minorVersion() >= 11)) {
        manager->copyObjectFromProcess(
                (remote_addr_t)(thread_addr + offsetof(Python3_11::PyThreadState, native_thread_id)),
                &the_tid);
    } else {
        the_tid = inferTidFromPThreadStructure(manager, pthread_id);
    }
    return the_tid;
}

int
PyThread::inferTidFromPThreadStructure(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        unsigned long pthread_id)
{
    // There is not a simple way of getting the Thread ID (tid) used by the OS
    // given the pthread_id (thread_id) that we just got from the remote process.
    // Turns out that the pthread id is just the address of the pthread struct
    // that is used to create the thread in the pthread library (this fact is used
    // by gdb and other debuggers). This struct contains the tid inside so we just
    // need to know the offset in this struct. The struct looks like this (from
    // glibc):
    //
    // struct pthread {
    //    union
    //    {
    //        tcbhead_t header;
    //        void *__padding[24];
    //    };
    //    list_t list;
    //    pid_t tid;
    //   ...
    //   }
    //
    int the_tid;
    manager->copyObjectFromProcess((remote_addr_t)(pthread_id + tid_offset_in_pthread_struct), &the_tid);

    // To double check that this number is correct, we then check that this is one
    // of the tids that we know. A thread id of 0 means that the thread was terminated
    // but not joined.
    const auto& tids = manager->Tids();
    if (the_tid != 0 && std::find(tids.begin(), tids.end(), the_tid) == tids.end()) {
        throw std::runtime_error("Invalid thread ID found!");
    }
    return the_tid;
}

remote_addr_t
PyThread::getFrameAddr(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        const PyThreadState& ts)
{
    if (manager->majorVersion() > 3 || (manager->majorVersion() == 3 && manager->minorVersion() >= 11)) {
        Python3_11::CFrame cframe;
        remote_addr_t cframe_addr =
                manager->versionedThreadField<remote_addr_t, &py_thread_v::o_frame>(ts);
        if (!manager->isAddressValid(cframe_addr)) {
            return reinterpret_cast<remote_addr_t>(nullptr);
        }
        manager->copyObjectFromProcess(cframe_addr, &cframe);
        return reinterpret_cast<remote_addr_t>(cframe.current_frame);
    } else {
        return manager->versionedThreadField<remote_addr_t, &py_thread_v::o_frame>(ts);
    }
}

std::shared_ptr<FrameObject>
PyThread::FirstFrame() const
{
    return d_first_frame;
}

std::shared_ptr<PyThread>
PyThread::NextThread() const
{
    return d_next;
}

PyThread::GilStatus
PyThread::isGilHolder() const
{
    return d_gil_status;
}

PyThread::GCStatus
PyThread::isGCCollecting() const
{
    return d_gc_status;
}

static inline bool
supportsExactGilChecking(int major, int minor)
{
    return major >= 3 && minor >= 8;
}

PyThread::GilStatus
PyThread::calculateGilStatus(const std::shared_ptr<const AbstractProcessManager>& manager) const
{
    LOG(DEBUG) << "Attempting to determine GIL Status";
    remote_addr_t thread_addr;
    remote_addr_t pyruntime = manager->findSymbol("_PyRuntime");
    if (pyruntime) {
        assert(manager->majorVersion() == 3);
        LOG(DEBUG) << "_PyRuntime symbol detected. Searching for GIL status within _PyRuntime structure";

        if (supportsExactGilChecking(manager->majorVersion(), manager->minorVersion())) {
            // Fast, exact method by checking the gilstate structure in _PyRuntime
            LOG(DEBUG) << "Searching for the GIL by checking the value of 'tstate_current'";
            PyRuntimeState runtime;
            manager->copyObjectFromProcess(pyruntime, &runtime);
            uintptr_t tstate_current =
                    manager->versionedRuntimeField<uintptr_t, &py_runtime_v::o_tstate_current>(runtime);
            return (tstate_current == d_addr ? GilStatus::HELD : GilStatus::NOT_HELD);
        } else {
            LOG(DEBUG) << "Searching for the GIL by scanning the _PyRuntime structure";
            // Slow, potentially unreliable method for older versions.
            // The thread object that has the GIL is stored twice at some unknown
            // offsets in the _PyRuntime structure. In order to determine if a given
            // thread has the GIL, we scan the _PyRuntime struct and check if the
            // address of the given thread object is present twice in the _PyRuntime
            // struct.
            int hits = 0;
            static const size_t MAX_RUNTIME_OFFSET = 2048;
            for (void** raddr = (void**)pyruntime;
                 (void*)raddr < (void*)(pyruntime + MAX_RUNTIME_OFFSET);
                 raddr++)
            {
                manager->copyObjectFromProcess((remote_addr_t)raddr, &thread_addr);
                if (thread_addr == d_addr && ++hits == 2) {
                    LOG(DEBUG) << "GIL status correctly determined: HELD";
                    return GilStatus::HELD;
                }
            }
            LOG(DEBUG) << "GIL status correctly determined: NOT HELD";
            return GilStatus::NOT_HELD;
        }
    } else {
        LOG(DEBUG) << "_PyRuntime symbol not detected. Searching for GIL status using "
                      "_PyThreadState_Current symbol";
        // Python 2 and older have a global symbol that holds the current thread
        // object (the one that has the GIL).
        remote_addr_t current_thread = manager->findSymbol("_PyThreadState_Current");
        if (current_thread) {
            manager->copyObjectFromProcess((remote_addr_t)current_thread, &thread_addr);
            LOG(DEBUG) << "GIL status correctly determined: " << (d_addr ? "HELD" : "NOT HELD");
            return thread_addr == d_addr ? GilStatus::HELD : GilStatus::NOT_HELD;
        }
    }
    LOG(DEBUG) << "Failed to determine the GIL status";
    return GilStatus::UNKNOWN;
}

PyThread::GCStatus
PyThread::calculateGCStatus(
        PyThreadState& ts,
        const std::shared_ptr<const AbstractProcessManager>& manager) const
{
    LOG(DEBUG) << "Attempting to determine GC Status";
    if (manager->majorVersion() < 3 || manager->minorVersion() < 7) {
        LOG(DEBUG) << "GC Status retrieval not supported by this Python version";
        return GCStatus::COLLECTING_UNKNOWN;
    }

    GCRuntimeState gcstate;

    switch (manager->minorVersion()) {
        case 11:
        case 10:
        case 9: {
            PyInterpreterState interp;
            auto is_addr = manager->versionedThreadField<remote_addr_t, &py_thread_v::o_interp>(ts);
            manager->copyObjectFromProcess(is_addr, &interp);
            gcstate = manager->versionedInterpreterStateField<GCRuntimeState, &py_is_v ::o_gc>(interp);
            break;
        }
        case 8:
        case 7: {
            remote_addr_t pyruntime = manager->findSymbol("_PyRuntime");
            if (!pyruntime) {
                LOG(DEBUG) << "Failed to get GC status because the _PyRuntime symbol is unavailable";
                return GCStatus::COLLECTING_UNKNOWN;
            }
            PyRuntimeState runtime;
            manager->copyObjectFromProcess(pyruntime, &runtime);
            gcstate = manager->versionedRuntimeField<GCRuntimeState, &py_runtime_v::o_gc>(runtime);
            break;
        }
        default: {
            LOG(DEBUG) << "GC Status retrieval not supported by this Python version";
            return GCStatus::COLLECTING_UNKNOWN;
        };
    }

    auto collecting = manager->versionedGcStatesField<remote_addr_t, &py_gc_v::o_collecting>(gcstate);
    LOG(DEBUG) << "GC status correctly retrieved: " << collecting;
    return collecting ? GCStatus::COLLECTING : GCStatus::NOT_COLLECTING;
}

// Create a similar funciton which does not pass the pointer to thread state, only the manager and the
// tid
std::shared_ptr<PyThread>
getThreadFromInterpreterState(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        remote_addr_t addr)
{
    if (tid_offset_in_pthread_struct == 0) {
        tid_offset_in_pthread_struct = findPthreadTidOffset(manager, addr);
    }

    LOG(DEBUG) << std::hex << std::showbase << "Copying PyInterpreterState struct from address " << addr;
    PyInterpreterState is;
    manager->copyObjectFromProcess(addr, &is);

    auto thread_addr =
            manager->versionedInterpreterStateField<remote_addr_t, &py_is_v::o_tstate_head>(is);
    return std::make_shared<PyThread>(manager, thread_addr);
}

}  // namespace pystack
