#include "thread_builder.h"

#include "logging.h"
#include "maps_parser.h"

namespace pystack {

// StackMethod flags (must match Python enum values)
enum StackMethodFlag {
    METHOD_ELF_DATA = 1 << 0,
    METHOD_SYMBOLS = 1 << 1,
    METHOD_BSS = 1 << 2,
    METHOD_ANONYMOUS_MAPS = 1 << 3,
    METHOD_HEAP = 1 << 4,
    METHOD_DEBUG_OFFSETS = 1 << 5,
};

std::vector<PyFrameData>
buildFrameStack(FrameObject* first_frame, bool resolve_locals)
{
    std::vector<PyFrameData> frames;
    FrameObject* current_frame = first_frame;

    while (current_frame != nullptr) {
        auto code = current_frame->Code();
        // Skip frames without code (shim frames) or with unreadable code ("???")
        if (!code || code->Filename() == "???") {
            auto prev = current_frame->PreviousFrame();
            current_frame = prev.get();
            continue;
        }

        if (resolve_locals) {
            current_frame->resolveLocalVariables();
        }

        PyFrameData frame_data;
        frame_data.code.filename = code->Filename();
        frame_data.code.scope = code->Scope();
        frame_data.code.location = code->Location();
        frame_data.arguments = current_frame->Arguments();
        frame_data.locals = current_frame->Locals();
        frame_data.is_entry = current_frame->IsEntryFrame();
        frame_data.is_shim = current_frame->IsShim();

        frames.push_back(std::move(frame_data));

        auto prev = current_frame->PreviousFrame();
        current_frame = prev.get();
    }

    return frames;
}

PyThreadData
buildPythonThread(
        const std::shared_ptr<AbstractProcessManager>& manager,
        PyThread* thread,
        pid_t pid,
        bool add_native_traces,
        bool resolve_locals)
{
    PyThreadData data;
    data.tid = thread->Tid();
    data.name = getThreadName(pid, thread->Tid());

    LOG(INFO) << "Constructing new Python thread with tid " << data.tid;

    if (add_native_traces) {
        thread->populateNativeStackTrace(manager);
    }

    auto first_frame = thread->FirstFrame();
    if (first_frame) {
        data.frames = buildFrameStack(first_frame.get(), resolve_locals);
    }

    const auto& native_frames = thread->NativeFrames();
    data.native_frames.assign(native_frames.rbegin(), native_frames.rend());

    data.gil_status = static_cast<int>(thread->isGilHolder());
    data.gc_status = static_cast<int>(thread->isGCCollecting());

    return data;
}

PyThreadData
buildNativeThread(const std::shared_ptr<AbstractProcessManager>& manager, pid_t pid, pid_t tid)
{
    PyThreadData data;
    data.tid = tid;
    data.name = getThreadName(pid, tid);
    data.gil_status = 0;  // NOT_HELD
    data.gc_status = 0;  // NOT_COLLECTING

    LOG(INFO) << "Constructing new native thread with tid " << tid;

    Thread native_thread(pid, tid);
    native_thread.populateNativeStackTrace(manager);

    const auto& native_frames = native_thread.NativeFrames();
    data.native_frames.assign(native_frames.rbegin(), native_frames.rend());

    return data;
}

std::vector<PyThreadData>
buildThreadsFromInterpreter(
        const std::shared_ptr<AbstractProcessManager>& manager,
        remote_addr_t interpreter_head,
        pid_t pid,
        bool add_native_traces,
        bool resolve_locals)
{
    LOG(INFO) << "Fetching Python threads";
    std::vector<PyThreadData> threads;

    auto thread = getThreadFromInterpreterState(manager, interpreter_head);
    PyThread* current_thread = thread.get();

    while (current_thread != nullptr) {
        threads.push_back(
                buildPythonThread(manager, current_thread, pid, add_native_traces, resolve_locals));

        auto next = current_thread->NextThread();
        current_thread = next.get();
    }

    return threads;
}

remote_addr_t
getInterpreterStateAddr(AbstractProcessManager* manager, int method_flags)
{
    remote_addr_t head = 0;

    struct MethodInfo
    {
        int flag;
        const char* name;
        std::function<remote_addr_t()> func;
    };

    std::vector<MethodInfo> methods = {
            {METHOD_DEBUG_OFFSETS,
             "using debug offsets data",
             [&]() { return manager->findInterpreterStateFromDebugOffsets(); }},
            {METHOD_ELF_DATA,
             "using ELF data",
             [&]() { return manager->findInterpreterStateFromElfData(); }},
            {METHOD_SYMBOLS,
             "using symbols",
             [&]() { return manager->findInterpreterStateFromSymbols(); }},
            {METHOD_BSS, "scanning the BSS", [&]() { return manager->scanBSS(); }},
            {METHOD_ANONYMOUS_MAPS,
             "scanning all anonymous maps",
             [&]() { return manager->scanAllAnonymousMaps(); }},
            {METHOD_HEAP, "scanning the heap", [&]() { return manager->scanHeap(); }},
    };

    for (const auto& method : methods) {
        if ((method_flags & method.flag) == 0) {
            continue;
        }

        try {
            head = method.func();
        } catch (const std::exception& exc) {
            LOG(WARNING) << "Unexpected error finding PyInterpreterState by " << method.name << ": "
                         << exc.what();
            continue;
        }

        if (head != 0) {
            LOG(INFO) << "PyInterpreterState found by " << method.name << " at address 0x" << std::hex
                      << head << std::dec;
            return head;
        } else {
            LOG(INFO) << "Address of PyInterpreterState not found by " << method.name;
        }
    }

    LOG(INFO) << "Address of PyInterpreterState could not be found";
    return 0;
}

std::vector<int>
getThreadIds(const std::shared_ptr<AbstractProcessManager>& manager)
{
    return manager->Tids();
}

}  // namespace pystack
