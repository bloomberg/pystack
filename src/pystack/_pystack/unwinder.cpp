#include <cassert>
#include <cstring>
#include <cxxabi.h>
#include <functional>
#include <optional>
#include <unistd.h>
#include <utility>

#include <dwarf.h>

#include "elf_common.h"
#include "logging.h"
#include "mem.h"
#include "native_frame.h"
#include "unwinder.h"

namespace pystack {
ModuleCuDieRanges::CuDieRanges::CuDieRanges(Dwfl_Module* mod)
{
    if (!mod) {
        return;
    }

    Dwarf_Die* die = nullptr;
    Dwarf_Addr bias = 0;
    while ((die = dwfl_module_nextcu(mod, die, &bias))) {
        Dwarf_Addr low = 0;
        Dwarf_Addr high = 0;
        Dwarf_Addr base = 0;
        ptrdiff_t offset = 0;
        while ((offset = dwarf_ranges(die, offset, &base, &low, &high)) > 0) {
            d_ranges.push_back(CuDieRange{die, bias, low + bias, high + bias});
        }
    }
}

Dwarf_Die*
ModuleCuDieRanges::CuDieRanges::findDie(Dwarf_Addr addr, Dwarf_Addr* bias) const
{
    auto it = std::find_if(d_ranges.begin(), d_ranges.end(), [addr](const CuDieRange& range) {
        return range.contains(addr);
    });
    if (it == d_ranges.end()) {
        return nullptr;
    }

    *bias = it->bias;
    return it->cuDie;
}

Dwarf_Die*
ModuleCuDieRanges::moduleAddrDie(Dwfl_Module* mod, Dwarf_Addr addr, Dwarf_Addr* bias)
{
    auto it = d_die_range_maps.find(mod);
    if (it == d_die_range_maps.end()) {
        it = d_die_range_maps.insert({mod, CuDieRanges(mod)}).first;
    }
    return it->second.findDie(addr, bias);
}

// AbstractUnwinder utilities

static std::optional<std::string>
DIENameFromScope(Dwarf_Die* die)
{
    Dwarf_Attribute attr;
    const char* name;
    name = dwarf_formstring(
            dwarf_attr_integrate(die, DW_AT_MIPS_linkage_name, &attr)
                    ?: dwarf_attr_integrate(die, DW_AT_linkage_name, &attr));
    if (name == nullptr) {
        name = dwarf_diename(die);
    }
    return name == nullptr ? std::nullopt : std::optional<std::string>(name);
}

static int
frameCallback(Dwfl_Frame* state, void* arg)
{
    auto* frames = static_cast<std::vector<Frame>*>(arg);
    Dwarf_Addr pc;
    bool isActivation;
    if (!dwfl_frame_pc(state, &pc, &isActivation)) {
        LOG(DEBUG) << "dwfl_frame_pc failed";
        return -1;
    }

    std::optional<Dwarf_Word> stackPointer;
    // Unwinding through musl libc with elfutils can get stuck returning the
    // same PC in a loop forever.
    //
    //   https://sourceware.org/bugzilla/show_bug.cgi?id=30272
    //   https://marc.info/?l=musl&m=168053842303968&w=2
    //
    // We can work around this by asking elfutils what the stack pointer is for
    // each frame and breaking out on our own if two different frames report
    // the same stack pointer. When PyStack is built with glibc and not built
    // with a recent enough version of elfutils for us to do this check, we
    // skip it. This isn't entirely correct, as it means that a PyStack built
    // with glibc and an old elfutils can fail to collect stacks for Python
    // interpreters built against musl libc, but it avoids a hard dependency on
    // a newer version of elfutils than most distros have available. It's
    // unlikely that users will encounter problems, but if they do, the simple
    // work around is to install PyStack using the same interpreter they want
    // to get stacks for, or to build with a more recent version of elfutils.

#if _ELFUTILS_VERSION >= 188 or (defined(__linux__) && !defined(__GLIBC__))

    // These platform specific magic numbers are part of the platform ABI.
    // For any platform not handled below we never look up the value of the
    // stack pointer register, and so never return DWARF_CB_ABORT.
    std::optional<unsigned int> stackPointerRegNo;
#    if defined(__x86_64__)
    // https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf
    // Figure 3.36: DWARF Register Number Mapping
    stackPointerRegNo = 7;
#    elif defined(__aarch64__)
    // https://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi.html#DW-REG
    stackPointerRegNo = 31;
#    endif

    if (stackPointerRegNo) {
        stackPointer.emplace(0);
        if (0 != dwfl_frame_reg(state, stackPointerRegNo.value(), &stackPointer.value())) {
            throw UnwinderError("Invalid register number!");
        }
    }

    if (!frames->empty() && pc == frames->back().pc && isActivation == frames->back().isActivation
        && stackPointer && stackPointer == frames->back().stackPointer)
    {
        LOG(DEBUG) << std::hex << std::showbase << "Breaking out of (infinite?) unwind loop @ " << pc;
        return DWARF_CB_ABORT;
    }
#endif

    frames->emplace_back(pc, isActivation, stackPointer);
    return DWARF_CB_OK;
}

static void
gatherInformationFromDIE(Dwarf_Die* cudie, Dwarf_Die* die, int* line, int* col, const char** sname)
{
    Dwarf_Files* files;
    if (dwarf_getsrcfiles(cudie, &files, nullptr) != 0) {
        return;
    }
    Dwarf_Attribute attr;
    Dwarf_Word val;
    if (dwarf_formudata(dwarf_attr(die, DW_AT_call_file, &attr), &val) != 0) {
        return;
    }
    *sname = dwarf_filesrc(files, val, nullptr, nullptr);
    if (dwarf_formudata(dwarf_attr(die, DW_AT_call_line, &attr), &val) != 0) {
        return;
    }
    *line = val;
    if (dwarf_formudata(dwarf_attr(die, DW_AT_call_column, &attr), &val) != 0) {
        return;
    }
    *col = val;
}

// AbstractUnwinder

std::pair<int, AbstractUnwinder::Scopes>
AbstractUnwinder::dwarfGetScopesDie(Dwarf_Die* die) const
{
    const auto elem = d_dwarf_getscopes_die_cache.find(die->addr);
    if (elem == d_dwarf_getscopes_die_cache.cend()) {
        Dwarf_Die* the_scopes;
        int nscopes = dwarf_getscopes_die(die, &the_scopes);
        auto pair = std::make_pair(nscopes, Scopes(the_scopes, std::free));
        d_dwarf_getscopes_die_cache.emplace(die->addr, pair);
        return pair;
    }
    return elem->second;
}

std::pair<int, AbstractUnwinder::Scopes>
AbstractUnwinder::dwarfGetScopes(Dwarf_Die* cudie, Dwarf_Addr pc) const
{
    const auto elem = d_dwarf_getscopes_cache.find(pc);
    if (elem == d_dwarf_getscopes_cache.cend()) {
        Dwarf_Die* _scopes = nullptr;
        int nscopes = dwarf_getscopes(cudie, pc, &_scopes);
        auto pair = std::make_pair(nscopes, Scopes(_scopes, std::free));
        d_dwarf_getscopes_cache.emplace(pc, pair);
        return pair;
    }
    return elem->second;
}

AbstractUnwinder::StatusCode
AbstractUnwinder::gatherInlineFrames(
        std::vector<NativeFrame>& native_frames,
        const std::string& noninline_symname,
        Dwarf_Addr pc,
        Dwarf_Addr pc_corrected,
        Dwarf_Die* cudie,
        const char* mod_name) const
{
    // Gather initial source information . The inline functions are chained in a
    // way that the symbol corresponding for a given scope represent the call that
    // is happening at that scope, which means that for retrieving the file name
    // and the source, we need to look at the previous scope, which is where the
    // call happened. The initial source information can be obtained from the
    // compilation unit itself.

    LOG(DEBUG) << std::hex << std::showbase << "Gathering inline frames for frame @ " << pc;

    auto srcloc = dwarf_getsrc_die(cudie, pc_corrected);
    if (!srcloc) {
        LOG(DEBUG) << std::hex << std::showbase << "Could not find main source information for PC @ "
                   << pc;
        LOG(DEBUG) << "Found non-inline call without source information: " << noninline_symname;
        native_frames.push_back({pc, demangleSymbol(noninline_symname), "???", 0, 0, mod_name});
        return StatusCode::ERROR;
    }

    const char* filename = nullptr;
    filename = dwarf_linesrc(srcloc, nullptr, nullptr);
    if (filename == nullptr) {
        filename = "???";
    }
    int line = 0;
    int col = 0;
    dwarf_lineno(srcloc, &line);
    dwarf_linecol(srcloc, &col);

    // Gather scope information for the given compilation unit
    const ScopesInfo cudie_scopes_info = dwarfGetScopes(cudie, pc_corrected);
    int ncudie_scopes = cudie_scopes_info.first;
    if (ncudie_scopes == 0) {
        LOG(DEBUG) << std::hex << std::showbase << "No inline scopes found for PC @ " << pc;
    } else {
        const Scopes& cudie_scopes = cudie_scopes_info.second;
        const ScopesInfo scopes_info = dwarfGetScopesDie(cudie_scopes.get());
        int nscopes = scopes_info.first;
        const Scopes& scopes = scopes_info.second;

        // Resolve all the inline frames in the obtained scopes

        for (int i = 0; i < nscopes; ++i) {
            Dwarf_Die* scope = &scopes.get()[i];
            if (dwarf_tag(scope) != DW_TAG_inlined_subroutine) {
                continue;
            }
            const std::optional<std::string> inlined_symname = DIENameFromScope(scope);
            if (!inlined_symname) {
                LOG(DEBUG) << "Scope with invalid name found @: " << scope->addr;
                continue;
            }
            LOG(DEBUG) << "Found inline call " << inlined_symname.value() << " @ " << filename << ":"
                       << line << ":" << col;
            native_frames.push_back(
                    {pc,
                     demangleSymbol(inlined_symname.value()) + " (inlined)",
                     filename ?: "???",
                     line,
                     col,
                     mod_name});
            gatherInformationFromDIE(cudie, scope, &line, &col, &filename);
        }
    }

    // Add the only non-inline call associated with this frame
    LOG(DEBUG) << "Found non-inline call " << noninline_symname << " @ " << filename << ":" << line
               << ":" << col;
    native_frames.push_back({pc, demangleSymbol(noninline_symname), filename, line, col, mod_name});
    return StatusCode::SUCCESS;
}

const char*
AbstractUnwinder::getNonInlineSymbolName(Dwfl_Module* mod, Dwarf_Addr pc) const
{
    auto item = d_symbol_by_pc_cache.find(pc);
    if (item != d_symbol_by_pc_cache.end()) {
        return item->second;
    }

    GElf_Sym sym;
    GElf_Off offset;
    const char* raw_symname = dwfl_module_addrinfo(mod, pc, &offset, &sym, nullptr, nullptr, nullptr);
    d_symbol_by_pc_cache.emplace(pc, raw_symname);
    return raw_symname;
}

std::vector<NativeFrame>
AbstractUnwinder::gatherFrames(const std::vector<Frame>& frames) const
{
    std::vector<NativeFrame> native_frames;
    for (auto& frame : frames) {
        LOG(DEBUG) << std::hex << std::showbase << "Resolving native information for frame @ "
                   << frame.pc;
        Dwarf_Addr pc = frame.pc;
        bool isactivation = frame.isActivation;
        Dwarf_Addr pc_adjusted = pc - (isactivation ? 0 : 1);

        Dwfl_Module* mod = dwfl_addrmodule(Dwfl(), pc_adjusted);
        const char* mod_name =
                dwfl_module_info(mod, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)
                        ?: "???";
        assert(mod_name != nullptr);
        LOG(DEBUG) << "Module identified for pc " << std::hex << std::showbase << pc << ": " << mod_name;
        const char* raw_symname = getNonInlineSymbolName(mod, pc);
        if (!raw_symname) {
            LOG(DEBUG) << std::hex << std::showbase << "Non-inline symbol name could not be resolved @ "
                       << pc;
            continue;
        }

        const std::string noninline_symbol = raw_symname;

        Dwarf_Addr bias = 0;
        Dwarf_Die* cudie = dwarfModuleAddrDie(pc_adjusted, mod, &bias);
        if (!cudie) {
            LOG(DEBUG) << std::hex << std::showbase << "Main compilation unit for pc " << pc << " ("
                       << noninline_symbol << ")"
                       << " could not be found";
            native_frames.push_back({pc, demangleSymbol(noninline_symbol), "???", 0, 0, mod_name});
            continue;
        }

        const auto pc_corrected = pc_adjusted - bias;
        gatherInlineFrames(native_frames, noninline_symbol, pc, pc_corrected, cudie, mod_name);
    }
    return native_frames;
}

Dwarf_Die*
AbstractUnwinder::dwarfModuleAddrDie(Dwarf_Addr pc_adjusted, Dwfl_Module* mod, Dwarf_Addr* bias) const
{
    Dwarf_Die* cudie = dwfl_module_addrdie(mod, pc_adjusted, bias);
    if (!cudie) {
        // Clang does produce suboptimal DWARF information, and in particular it
        // does not produce the ARANGES attribute in the DIE information, preventing
        // the previous call to work. To work around this, we need to scan
        // everything ourselves manually to reconstruct the missing information.
        cudie = d_range_maps_cache.moduleAddrDie(mod, pc_adjusted, bias);
    }
    return cudie;
}

struct ModuleArg
{
    const char* symbol;
    const char* modulename;
    remote_addr_t addr;
};

static int
module_callback(
        Dwfl_Module* mod,
        void** userdata __attribute__((unused)),
        const char* name __attribute__((unused)),
        Dwarf_Addr start __attribute__((unused)),
        void* arg)
{
    auto module_arg = static_cast<ModuleArg*>(arg);
    //    std::cerr << "Searching in " << name << std::endl;
    if (strstr(module_arg->modulename, name) == nullptr) {
        LOG(DEBUG) << "Skipping map for symbols " << name << " because doesn't match "
                   << module_arg->modulename;
        return DWARF_CB_OK;
    }
    LOG(INFO) << "Attempting to find symbol '" << module_arg->symbol << "' in " << name;
    int n_syms = dwfl_module_getsymtab(mod);
    if (n_syms == -1) {
        return DWARF_CB_OK;
    }
    GElf_Sym sym;
    GElf_Addr addr;
    for (int i = 0; i < n_syms; i++) {
        const char* sname = dwfl_module_getsym_info(mod, i, &sym, &addr, nullptr, nullptr, nullptr);
        if (strcmp(sname, module_arg->symbol) == 0) {
            module_arg->addr = addr;
            LOG(INFO) << "Symbol '" << sname << "' found at address " << std::hex << std::showbase
                      << addr;
            break;
        }
    }
    return DWARF_CB_OK;
}

remote_addr_t
AbstractUnwinder::getAddressforSymbol(const std::string& symbol, const std::string& modulename) const
{
    LOG(DEBUG) << "Trying to find address for symbol " << symbol;
    ModuleArg arg = {symbol.c_str(), modulename.c_str(), 0};
    if (dwfl_getmodules(Dwfl(), module_callback, &arg, 0) != 0) {
        throw UnwinderError("Failed to fetch modules!");
    }
    LOG(DEBUG) << "Address for symbol " << symbol << " resolved to: " << std::hex << std::showbase
               << arg.addr;
    return arg.addr;
}

std::string
AbstractUnwinder::demangleSymbol(const std::string& symbol)
{
    // Require GNU v3 ABI by the "_Z" prefix.
    if (symbol[0] != '_' || symbol[1] != 'Z') {
        LOG(DEBUG) << "Symbol " << symbol << " cannot be demangled";
        return symbol;
    }
    int status = -1;
    char* dsymname = abi::__cxa_demangle(symbol.c_str(), nullptr, nullptr, &status);
    if (status != 0) {
        LOG(DEBUG) << "Failed to demangle symbol " << symbol << " with error: " << status;
        return symbol;
    }
    LOG(DEBUG) << "Successfully demangled symbol " << symbol << " to: " << dsymname;
    std::string new_symbol(dsymname);
    free(dsymname);
    return new_symbol;
}

Unwinder::Unwinder(std::shared_ptr<ProcessAnalyzer> analyzer)
: d_analyzer(std::move(analyzer))
{
}

Dwfl*
Unwinder::Dwfl() const
{
    return d_analyzer->d_dwfl.get();
}

std::vector<NativeFrame>
Unwinder::unwindThread(pid_t tid) const
{
    LOG(DEBUG) << "Unwinding frames for tid: " << tid;
    std::vector<Frame> frames;
    if (!tid) {
        LOG(ERROR) << "Cannot unwind thread due to invalid tid: " << tid;
        return {};
    }
    switch (dwfl_getthread_frames(Dwfl(), tid, frameCallback, (void*)(&frames))) {
        case DWARF_CB_OK:
        case DWARF_CB_ABORT:
            break;
        case -1:
            // This may or may not be an error, as it can signal the end of the stack
            // unwinding.
            if (frames.empty()) {
                int dwfl_err = dwfl_errno();
                std::string error(
                        dwfl_err ? dwfl_errmsg(dwfl_err) : "unwinding failed with no error reported");
                throw UnwinderError("Unknown error happened when gathering thread frames: " + error);
            }
            break;
        default:
            throw UnwinderError("Unknown error happened when gathering thread frames");
    }
    return gatherFrames(frames);
}

CoreFileUnwinder::CoreFileUnwinder(std::shared_ptr<CoreFileAnalyzer> analyzer)
: d_analyzer(std::move(analyzer))
{
}

static int
thread_callback(Dwfl_Thread* thread, void* thread_arg)
{
    auto tids = static_cast<std::vector<int>*>(thread_arg);
    tids->emplace_back(dwfl_thread_tid(thread));
    return DWARF_CB_OK;
}

struct ThreadArg
{
    pid_t tid;
    std::vector<Frame>& frames;
};

static int
thread_callback_for_frames(Dwfl_Thread* thread, void* arg)
{
    auto* thread_arg = static_cast<ThreadArg*>(arg);
    pid_t tid = dwfl_thread_tid(thread);
    if (tid != thread_arg->tid) {
        return DWARF_CB_OK;
    }

    switch (dwfl_thread_getframes(thread, frameCallback, (void*)(&(thread_arg->frames)))) {
        case DWARF_CB_OK:
        case DWARF_CB_ABORT:
            break;
        case -1:
            // This may or may not be an error, as it can signal the end of the stack
            // unwinding.
            if (thread_arg->frames.empty()) {
                int dwfl_err = dwfl_errno();
                std::string error(
                        dwfl_err ? dwfl_errmsg(dwfl_err) : "unwinding failed with no error reported");
                throw UnwinderError("Unknown error happened when gathering thread frames: " + error);
            }
            break;
        default:
            throw UnwinderError("Unknown error happened when gathering thread frames");
    }
    return DWARF_CB_OK;
}

std::vector<NativeFrame>
CoreFileUnwinder::unwindThread(pid_t tid) const
{
    LOG(DEBUG) << "Unwinding frames for tid: " << tid;
    if (!tid) {
        LOG(ERROR) << "Cannot unwind thread due to invalid tid: " << tid;
        return {};
    }
    std::vector<Frame> frames;
    ThreadArg args = {tid, frames};
    // When unwinding core files, we cannot use dwfl_thread_getframes to inspect a
    // single thread because libdwfl leaks memory otherwise (dwfl_thread_getframes
    // is not supposed to be used directly with core files). So we need to inspect
    // frame by frame and skip the ones that are different from *tid*.
    switch (dwfl_getthreads(Dwfl(), thread_callback_for_frames, (void*)(&args))) {
        case DWARF_CB_OK:
        case DWARF_CB_ABORT:
            break;
        case -1:
            // This may or may not be an error, as it can signal the end of the stack
            // unwinding.
            if (frames.empty()) {
                int dwfl_err = dwfl_errno();
                std::string error(
                        dwfl_err ? dwfl_errmsg(dwfl_err) : "unwinding failed with no error reported");
                throw UnwinderError("Unknown error happened when gathering thread frames: " + error);
            }
            break;
        default:
            throw UnwinderError("Unknown error happened when gathering thread frames");
    }
    return gatherFrames(frames);
}

std::vector<int>
CoreFileUnwinder::getCoreTids() const
{
    std::vector<int> tids;
    if (dwfl_getthreads(Dwfl(), thread_callback, &tids)) {
        throw UnwinderError("Failed to get program headers");
    }
    return tids;
}

struct Dwfl*
CoreFileUnwinder::Dwfl() const
{
    return d_analyzer->d_dwfl.get();
}
}  // namespace pystack
