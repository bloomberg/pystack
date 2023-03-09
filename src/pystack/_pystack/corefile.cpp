#include <cassert>
#include <cstring>
#include <fstream>
#include <inttypes.h>
#include <utility>
#include <vector>

#include "corefile.h"
#include "elf_common.h"
#include "libelf.h"
#include "logging.h"
#include <csignal>
#include <gelf.h>
#include <sys/procfs.h>

namespace pystack {

enum class StatusCode {
    SUCCESS,
    ERROR,
};

// These constants are not always defined in the system headers. Their value is obtained from the kernel
// sources:
// https://github.com/torvalds/linux/blob/1e6d1d96461eb350a98c1a0fe9fd93ea14a157e8/include/uapi/linux/elf.h#L382-L383
#ifndef NT_SIGINFO
static const int NT_SIGINFO = 0x53494749;
#endif
#ifndef NT_FILE
static const int NT_FILE = 0x46494c45;
#endif

struct DwarfModuleInformation
{
    Dwarf_Addr start;
    Dwarf_Addr end;
    std::string filename;
};

static int
module_callback(
        Dwfl_Module* mod,
        void** userdata __attribute__((unused)),
        const char* name __attribute__((unused)),
        Dwarf_Addr starty __attribute__((unused)),
        void* arg)
{
    Dwarf_Addr start;
    Dwarf_Addr end;
    const char* mainfile;
    const char* debugfile;
    const char* modname =
            dwfl_module_info(mod, nullptr, &start, &end, nullptr, nullptr, &mainfile, &debugfile);
    if (mainfile != nullptr) {
        modname = mainfile;
    } else if (debugfile != nullptr) {
        modname = debugfile;
    }
    const unsigned char* id;
    GElf_Addr id_vaddr;
    std::string buildid = "";
    int ret = dwfl_module_build_id(mod, &id, &id_vaddr);
    if (ret > 0) {
        buildid = buildIdPtrToString(id, ret);
    }

    SimpleVirtualMap inf = {start, end, modname, buildid};
    auto module_info = static_cast<std::vector<SimpleVirtualMap>*>(arg);

    LOG(DEBUG) << "Found debug info for module " << (modname == nullptr ? "???" : modname)
               << " spanning from " << std::hex << std::showbase << start << " to " << end;
    module_info->push_back(inf);

    return DWARF_CB_OK;
}

void
CoreFileExtractor::populateMaps()
{
    LOG(DEBUG) << "Populating memory maps for core file";
    /* Check that we are working with a coredump. */
    GElf_Ehdr ehdr;
    if (gelf_getehdr(d_analyzer->d_elf.get(), &ehdr) == nullptr || ehdr.e_type != ET_CORE) {
        throw CoreAnalyzerError("The file is not a coredump!");
    }

    if (dwfl_getmodules(d_analyzer->d_dwfl.get(), module_callback, &d_module_info, 0) != 0) {
        throw CoreAnalyzerError("Failed to fetch modules!");
    }

    std::for_each(d_module_info.begin(), d_module_info.end(), [this](auto& mod) {
        std::string relocated_library = d_analyzer->locateLibrary(mod.filename);
        LOG(DEBUG) << "Resolved library " << mod.filename << " to " << relocated_library;
        mod.filename = relocated_library;
    });

    size_t nphdr;
    if (elf_getphdrnum(d_analyzer->d_elf.get(), &nphdr) != 0) {
        throw CoreAnalyzerError("Failed to get program headers");
    }

    std::vector<CoreVirtualMap> vmaps;

    LOG(DEBUG) << "Found " << nphdr << " program headers";
    LOG(DEBUG) << "Searching for PT_LOAD segments";

    for (size_t i = 0; i < nphdr; i++) {
        GElf_Phdr phdr;
        if (gelf_getphdr(d_analyzer->d_elf.get(), i, &phdr) != &phdr) {
            continue;
        }

        if (phdr.p_type != PT_LOAD) {
            continue;
        }

        Dwarf_Addr start = phdr.p_vaddr;
        Dwarf_Addr end = phdr.p_vaddr + phdr.p_memsz;

        auto it = std::find_if(d_module_info.cbegin(), d_module_info.cend(), [&](auto& module) {
            return start >= module.start && end <= module.end;
        });

        std::string filename;
        if (it != d_module_info.cend()) {
            filename = it->filename;
        }
        LOG(DEBUG) << "Found PT_LOAD segment for module " << (filename.empty() ? "???" : filename)
                   << " spanning from " << std::hex << std::showbase << start << " to " << end;

        std::string build_id = getBuildId(filename);

        CoreVirtualMap vmap = {
                start,
                end,
                phdr.p_filesz,
                parse_permissions(phdr.p_flags),
                phdr.p_offset,
                "",
                0,
                filename,
                build_id};

        d_maps.push_back(vmap);
    }
}

CoreFileExtractor::CoreFileExtractor(std::shared_ptr<CoreFileAnalyzer> analyzer)
: d_analyzer(std::move(analyzer))
{
    populateMaps();
}

std::vector<CoreVirtualMap>
CoreFileExtractor::MemoryMaps() const
{
    return d_maps;
}

std::vector<SimpleVirtualMap>
CoreFileExtractor::ModuleInformation() const
{
    return d_module_info;
}

pid_t
CoreFileExtractor::Pid() const
{
    return d_analyzer->d_pid;
}

std::string
CoreFileExtractor::extractExecutable() const
{
    uintptr_t addr = findExecFn();

    LOG(DEBUG) << std::hex << std::showbase << "Found exec_fn attribute at address: " << std::hex
               << std::showbase << addr;

    if (!addr) {
        throw ElfAnalyzerError("Failed to locate the address of the executable string in the core file");
    }

    auto it = std::find_if(d_maps.cbegin(), d_maps.cend(), [&](auto& map) {
        return map.start <= addr && addr <= map.end;
    });

    if (it == d_maps.cend()) {
        throw ElfAnalyzerError(
                "Failed to locate the map where the executable string resides in the core file");
    }

    unsigned long location = addr - it->start + it->offset;
    std::ifstream is(d_analyzer->d_filename, std::ifstream::binary);
    if (!is) {
        throw ElfAnalyzerError("Failed to open the core file for analysis");
    }
    is.seekg(location);
    std::string executable_string;
    std::getline(is, executable_string, '\0');
    LOG(DEBUG) << "Executable string (exec_fn) extracted from core file: " << executable_string;
    return executable_string;
}

static auto
read_obj(const char** source, void* dest, size_t size)
{
    ::memcpy(dest, *source, size);
    *source += size;
}

static StatusCode
parseCorePsinfo(const NoteData& note_data, CorePsInfo* result)
{
    if (note_data.data->d_size != sizeof(CorePsInfo)) {
        LOG(ERROR) << "Invalid psinfo note found";
        return StatusCode::ERROR;
    }
    std::memcpy(result, note_data.data->d_buf, sizeof(CorePsInfo));
    // Ensure that the char arrays are null terminated
    result->fname[FNAME_SIZE - 1] = '\0';
    result->psargs[PSARGS_SIZE - 1] = '\0';
    return StatusCode::SUCCESS;
}

static StatusCode
parseCorePrstatus(const NoteData& note_data, CoreCrashInfo* result)
{
    if (note_data.data->d_size != sizeof(prstatus_t)) {
        LOG(ERROR) << "Invalid prstatus note found";
        return StatusCode::ERROR;
    }
    auto* prstatus = reinterpret_cast<prstatus_t*>(note_data.data->d_buf);
    *result = {prstatus->pr_info.si_signo, prstatus->pr_info.si_errno, prstatus->pr_info.si_code};
    return StatusCode::SUCCESS;
}

static StatusCode
parseCoreSiginfo(const NoteData& note_data, CoreCrashInfo* result)
{
    if (note_data.data->d_size != sizeof(siginfo_t)) {
        LOG(ERROR) << "Invalid siginfo note found";
        return StatusCode::ERROR;
    }

    const size_t int_size = gelf_fsize(note_data.elf, ELF_T_WORD, 1, EV_CURRENT);
    assert(int_size > 0);

    const char* ptr = static_cast<const char*>(note_data.data->d_buf);
    read_obj(&ptr, &result->si_signo, int_size);
    read_obj(&ptr, &result->si_errno, int_size);
    read_obj(&ptr, &result->si_code, int_size);

    // We need to account for alignment in 64 bits
    if (gelf_getclass(note_data.elf) == ELFCLASS64) {
        ptr += 4;
    }

    if (result->si_code > 0) {
        switch (result->si_signo) {
            case SIGILL:
            case SIGFPE:
            case SIGSEGV:
            case SIGBUS: {
                const size_t addr_size = gelf_fsize(note_data.elf, ELF_T_ADDR, 1, EV_CURRENT);
                assert(addr_size > 0);
                read_obj(&ptr, &result->failed_addr, addr_size);
                break;
            }
            default:
                break;
        }
    } else if (result->si_code == SI_USER) {
        read_obj(&ptr, &result->sender_pid, int_size);
        read_obj(&ptr, &result->sender_uid, int_size);
    }
    assert(ptr <= reinterpret_cast<char*>(note_data.data->d_buf) + note_data.data->d_size);
    return StatusCode::SUCCESS;
}

static StatusCode
parseCoreFileNote(Elf* core, const NoteData& note_data, std::vector<CoreVirtualMap>& result)
{
    Elf_Data* data = note_data.data;
    assert(data != NULL);

    const size_t ulong_size = gelf_fsize(note_data.elf, ELF_T_ADDR, 1, EV_CURRENT);
    if (ulong_size <= 0) {
        LOG(ERROR) << "Cannot determine the size of 'long' for ELF file";
        return StatusCode::ERROR;
    }
    const char* ptr = static_cast<const char*>(data->d_buf);
    const char* end = static_cast<const char*>(data->d_buf) + data->d_size;

    uint64_t count, page_size;
    read_obj(&ptr, &count, ulong_size);
    read_obj(&ptr, &page_size, ulong_size);

    size_t addrsize = gelf_fsize(core, ELF_T_ADDR, 1, EV_CURRENT);
    size_t entry_size = 3 * addrsize;  // mstart, mend, moffset
    uint64_t maxcount = (size_t)(end - ptr) / entry_size;
    if (count > maxcount) {
        LOG(ERROR) << "Failed to parse file note data: invalid number of entries";
        return StatusCode::ERROR;
    }

    // File names are stored at the end of the main table
    const char* filename_table_start = ptr + count * entry_size;
    const char* filename_table_ptr = filename_table_start;

    for (size_t i = 0; i < count; ++i) {
        // Read the data for a single entry
        uint64_t mstart, mend, moffset;
        read_obj(&ptr, &mstart, ulong_size);
        read_obj(&ptr, &mend, ulong_size);
        read_obj(&ptr, &moffset, ulong_size);

        // Fetch the corresponding file name from the file name table
        std::string filename(filename_table_ptr);
        result.emplace_back(CoreVirtualMap{mstart, mend, 0, "", moffset * page_size, "", 0, filename});

        // Advance the file name table pointer
        const char* next_filename =
                static_cast<const char*>(memchr(filename_table_ptr, '\0', end - filename_table_ptr));
        if (next_filename == nullptr) {
            LOG(ERROR) << "Failed to parse file note data: file name table ended too soon";
            return StatusCode::ERROR;
        }
        filename_table_ptr = next_filename + 1;
    }
    return StatusCode::SUCCESS;
}

const std::vector<CoreVirtualMap>
CoreFileExtractor::extractMappedFiles() const
{
    Elf* elf = d_analyzer->d_elf.get();
    std::vector<CoreVirtualMap> result;
    LOG(DEBUG) << "Extracting mapped files from core file note";

    for (const auto& note_data : getNoteData(elf, NT_FILE, ELF_T_XWORD)) {
        if (parseCoreFileNote(elf, note_data, result) != StatusCode::ERROR) {
            LOG(DEBUG) << "Mapped files found in core file note";
            return result;
        }
    }
    LOG(DEBUG) << "Mapped files could not be found in core file note";
    return result;
}

CoreCrashInfo
CoreFileExtractor::extractFailureInfo() const
{
    Elf* elf = d_analyzer->d_elf.get();
    CoreCrashInfo result{};

    LOG(DEBUG) << "Extracting failure info structure";

    LOG(DEBUG) << "Checking for NT_SIGINFO section";
    // Check first if we have a NT_SIGINFO section, as more information can be retrieved from it
    for (const auto& note_data : getNoteData(elf, NT_SIGINFO, ELF_T_XWORD)) {
        if (parseCoreSiginfo(note_data, &result) != StatusCode::ERROR) {
            LOG(DEBUG) << "NT_SIGINFO found";
            return result;
        }
    }

    // Fallback to the NT_PRSTATUS section (even old kernels should always produce this).
    LOG(DEBUG) << "Checking for NT_PRSTATUS section";
    for (const auto& note_data : getNoteData(elf, NT_PRSTATUS, ELF_T_XWORD)) {
        if (parseCorePrstatus(note_data, &result) != StatusCode::ERROR) {
            LOG(DEBUG) << "NT_PRSTATUS found";
            return result;
        }
    }
    LOG(DEBUG) << "Failed to locate the NOTE data for the failure info in the core file";
    return {};
}

CorePsInfo
CoreFileExtractor::extractPSInfo() const
{
    Elf* elf = d_analyzer->d_elf.get();
    CorePsInfo result{};
    LOG(DEBUG) << "Extracting PSInfo structure";

    for (const auto& note_data : getNoteData(elf, NT_PRPSINFO, ELF_T_XWORD)) {
        if (parseCorePsinfo(note_data, &result) != StatusCode::ERROR) {
            LOG(DEBUG) << "PSInfo structure found";
            return result;
        }
    }
    LOG(ERROR) << "Failed to locate the NOTE data for the psinfo struct in the core file";
    return {};
}

static StatusCode
parseCoreExecfn(const NoteData& note_data, uintptr_t* result)
{
    const size_t auxv_size = gelf_fsize(note_data.elf, ELF_T_AUXV, 1, EV_CURRENT);
    assert(auxv_size > 0);
    const size_t nauxv = note_data.descriptor_size / auxv_size;
    for (size_t i = 0; i < nauxv; ++i) {
        GElf_auxv_t av_mem;
        const GElf_auxv_t* av = gelf_getauxv(note_data.data, i, &av_mem);
        if (!av || av->a_type != AT_EXECFN) {
            continue;
        }
        *result = static_cast<uintptr_t>(av->a_un.a_val);
        return StatusCode::SUCCESS;
    }
    return StatusCode::ERROR;
}

uintptr_t
CoreFileExtractor::findExecFn() const
{
    LOG(DEBUG) << "Extracting ExecFn information";
    // If we have section headers, look for SHT_NOTE sections.
    // In a core file, the program headers may not be reliable.

    Elf* elf = d_analyzer->d_elf.get();
    uintptr_t result = 0;
    size_t shnum;

    if (elf_getshdrnum(elf, &shnum) < 0) {
        LOG(ERROR) << "Cannot determine the number of sections in the ELF file";
        return (uintptr_t) nullptr;
    }

    LOG(DEBUG) << "Found " << shnum << " sections in the ELF file";

    if (shnum != 0) {
        Elf_Scn* scn = nullptr;
        while ((scn = elf_nextscn(elf, scn)) != nullptr) {
            GElf_Shdr shdr_mem;
            GElf_Shdr* shdr = gelf_getshdr(scn, &shdr_mem);
            if (shdr == nullptr || shdr->sh_type != SHT_NOTE) {
                continue;
            }
            LOG(DEBUG) << "Valid SHT_NOTE segment found with offset " << std::hex << std::showbase
                       << shdr->sh_offset << ". Attempting to get ExecDn structure";
            Elf_Data* data = elf_getdata(scn, nullptr);
            const NoteData note_data{elf, data, shdr->sh_offset};
            if (parseCoreExecfn(note_data, &result) != StatusCode::ERROR) {
                LOG(DEBUG) << "ExecFn structure found";
                return result;
            }
        }
    }

    LOG(DEBUG) << "Failed to locate the NOTE section via section headers";
    LOG(DEBUG) << "Attempting to get ExecFn from auxiliary vector";

    for (const auto& note_data : getNoteData(elf, NT_AUXV, ELF_T_AUXV)) {
        if (parseCoreExecfn(note_data, &result) != StatusCode::ERROR) {
            LOG(DEBUG) << "ExecFn structure found";
            return result;
        }
    }
    LOG(ERROR) << "Failed to extract the ExecFn information from the core file";
    return (uintptr_t) nullptr;
}
std::vector<std::string>
CoreFileExtractor::missingModules() const
{
    return d_analyzer->d_missing_modules;
}

}  // namespace pystack
