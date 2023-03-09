#pragma once
#include "memory"
#include <memory>
#include <sys/types.h>
#include <vector>

#include "mem.h"
#include "native_frame.h"
#include "process.h"
#include "pyframe.h"

namespace pystack {

class Thread
{
  public:
    Thread(pid_t pid, pid_t tid);
    pid_t Tid() const;
    const std::vector<NativeFrame>& NativeFrames() const;

    // Methods
    void populateNativeStackTrace(const std::shared_ptr<const AbstractProcessManager>& manager);

  protected:
    // Data members
    pid_t d_pid;
    pid_t d_tid;
    std::vector<NativeFrame> d_native_frames;
};

class PyThread : public Thread
{
  public:
    // Enums
    enum GilStatus { UNKNOWN = -1, NOT_HELD = 0, HELD = 1 };
    enum GCStatus { COLLECTING_UNKNOWN = -1, NOT_COLLECTING = 0, COLLECTING = 1 };

    // Constructors
    PyThread(const std::shared_ptr<const AbstractProcessManager>& manager, remote_addr_t addr);

    // Getters
    std::shared_ptr<FrameObject> FirstFrame() const;
    std::shared_ptr<PyThread> NextThread() const;

    // Methods
    GilStatus isGilHolder() const;
    GCStatus isGCCollecting() const;

    // Static Methods
    static remote_addr_t
    getFrameAddr(const std::shared_ptr<const AbstractProcessManager>& manager, const PyThreadState& ts);

  private:
    // Data members
    unsigned long d_pthread_id;
    GilStatus d_gil_status;
    GCStatus d_gc_status;
    remote_addr_t d_addr;
    remote_addr_t d_next_addr;
    std::shared_ptr<PyThread> d_next;
    std::shared_ptr<FrameObject> d_first_frame;

    // Methods
    GilStatus calculateGilStatus(const std::shared_ptr<const AbstractProcessManager>& manager) const;
    GCStatus calculateGCStatus(
            PyThreadState& ts,
            const std::shared_ptr<const AbstractProcessManager>& manager) const;

    // Static Methods
    static int inferTidFromPThreadStructure(
            const std::shared_ptr<const AbstractProcessManager>& manager,
            unsigned long pthread_id);
    static int getThreadTid(
            const std::shared_ptr<const AbstractProcessManager>& manager,
            remote_addr_t thread_addr,
            unsigned long pthread_id);
};

std::shared_ptr<PyThread>
getThreadFromInterpreterState(
        const std::shared_ptr<const AbstractProcessManager>& manager,
        remote_addr_t addr);
}  // namespace pystack
