#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "maps_parser.h"
#include "native_frame.h"
#include "process.h"
#include "pycode.h"
#include "pyframe.h"
#include "pythread.h"

namespace pystack {

struct PyCodeData
{
    std::string filename;
    std::string scope;
    LocationInfo location;
};

struct PyFrameData
{
    PyCodeData code;
    std::unordered_map<std::string, std::string> arguments;
    std::unordered_map<std::string, std::string> locals;
    bool is_entry;
    bool is_shim;
};

struct PyThreadData
{
    int tid;
    std::optional<std::string> name;
    std::vector<PyFrameData> frames;
    std::vector<NativeFrame> native_frames;
    int gil_status;  // -1 = unknown, 0 = not held, 1 = held
    int gc_status;  // -1 = unknown, 0 = not collecting, 1 = collecting
};

std::vector<PyThreadData>
buildThreadsFromInterpreter(
        const std::shared_ptr<AbstractProcessManager>& manager,
        remote_addr_t interpreter_head,
        pid_t pid,
        bool add_native_traces,
        bool resolve_locals);

PyThreadData
buildPythonThread(
        const std::shared_ptr<AbstractProcessManager>& manager,
        PyThread* thread,
        pid_t pid,
        bool add_native_traces,
        bool resolve_locals);

PyThreadData
buildNativeThread(const std::shared_ptr<AbstractProcessManager>& manager, pid_t pid, pid_t tid);

std::vector<PyFrameData>
buildFrameStack(FrameObject* first_frame, bool resolve_locals);

remote_addr_t
getInterpreterStateAddr(AbstractProcessManager* manager, int method_flags);

std::vector<int>
getThreadIds(const std::shared_ptr<AbstractProcessManager>& manager);

}  // namespace pystack
