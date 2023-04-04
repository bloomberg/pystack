#include <cassert>
#include <cstring>
#include <inttypes.h>
#include <iomanip>
#include <iostream>
#include <utility>

#include "compat.h"
#include "elf_common.h"

namespace pystack {

using file_unique_ptr = std::unique_ptr<FILE, std::function<int(FILE*)>>;

int
pystack_find_elf(
        Dwfl_Module* mod,
        void** userdata,
        const char* modname,
        Dwarf_Addr base,
        char** file_name,
        Elf** elfp)
{
    const char* the_modname = (modname == nullptr) ? "???" : modname;
    int ret = dwfl_build_id_find_elf(mod, userdata, modname, base, file_name, elfp);
    if (ret > 0) {
        const char* the_filename = (*file_name == nullptr) ? "???" : *file_name;
        LOG(DEBUG) << "Located debug info for " << the_modname << " using BUILD ID in " << the_filename;
        return ret;
    }
    ret = dwfl_linux_proc_find_elf(mod, userdata, modname, base, file_name, elfp);
    if (file_name == nullptr) {
        LOG(DEBUG) << "Could not locate debug info for " << the_modname;
    } else {
        LOG(DEBUG) << "Located debug info for " << the_modname << " by path in " << *file_name;
    }
    return ret;
}

std::string
parse_permissions(long flags)
{
    std::string perms;
    if (flags & PF_R) {
        perms += "r";
    }
    if (flags & PF_W) {
        perms += "w";
    }
    if (flags & PF_X) {
        perms += "x";
    }
    return perms;
}

CoreFileAnalyzer::CoreFileAnalyzer(
        std::string corefile,
        std::optional<std::string> executable,
        const std::optional<std::string>& lib_search_path)
: d_dwfl(nullptr)
, d_debuginfo_path(nullptr)
, d_callbacks()
, d_filename(std::move(corefile))
, d_executable(std::move(executable))
, d_lib_search_path(std::move(lib_search_path))
, d_fd(0)
, d_elf(nullptr)
{
    if (elf_version(EV_CURRENT) == EV_NONE) {
        throw ElfAnalyzerError("libelf library ELF version too old");
    }

    d_fd = open(d_filename.c_str(), O_RDONLY);
    if (d_fd == -1) {
        throw ElfAnalyzerError("Failed to open ELF file " + d_filename);
    }

    d_elf = elf_unique_ptr(elf_begin(d_fd, ELF_C_READ_MMAP, nullptr), elf_end);
    if (!d_elf) {
        close(d_fd);
        throw ElfAnalyzerError("Cannot read elf file");
    }

    std::memset(&d_callbacks, 0, sizeof(d_callbacks));
    d_callbacks.find_elf = pystack_find_elf;
    d_callbacks.find_debuginfo = dwfl_standard_find_debuginfo;
    d_callbacks.debuginfo_path = &d_debuginfo_path;

    d_dwfl = dwfl_unique_ptr(dwfl_begin(&d_callbacks), dwfl_end);

    if (!d_dwfl) {
        throw ElfAnalyzerError("Failed to initialize core analyzer");
    }

    const char* the_executable = d_executable.has_value() ? d_executable.value().c_str() : nullptr;

    if (dwfl_core_file_report(d_dwfl.get(), d_elf.get(), the_executable) < 0
        || dwfl_report_end(d_dwfl.get(), nullptr, nullptr) != 0)
    {
        throw ElfAnalyzerError(
                "Failed to analyze DWARF information for the core file. '" + d_filename
                + "' doesn't look like a valid core file.");
    }

    resolveLibraries();

    int result = dwfl_core_file_attach(d_dwfl.get(), d_elf.get());
    if (result < 0) {
        throw ElfAnalyzerError(
                "Could not attach the core map analyzer. '" + d_filename
                + "' doesn't look like a valid core file.");
    }
    d_pid = result;
}

CoreFileAnalyzer::~CoreFileAnalyzer()
{
    close(d_fd);
}

void
CoreFileAnalyzer::removeModuleIf(std::function<bool(Dwfl_Module*)> predicate) const
{
    using Predicate = decltype(predicate);
    struct CallbackArgs
    {
        Dwfl* dwfl;
        Predicate& predicate;
    } callback_args = {d_dwfl.get(), predicate};

    // Remove all modules, except for any that the callback re-adds.
    dwfl_report_begin(d_dwfl.get());

    int const rc = dwfl_report_end(
            d_dwfl.get(),
            [](Dwfl_Module* mod, void*, const char* name, Dwarf_Addr start, void* arg) -> int {
                auto& callback_args = *static_cast<CallbackArgs*>(arg);
                if (!callback_args.predicate(mod)) {
                    Dwarf_Addr end;
                    dwfl_module_info(mod, nullptr, nullptr, &end, nullptr, nullptr, nullptr, nullptr);
                    if (!dwfl_report_module(callback_args.dwfl, name, start, end)) {
                        throw ElfAnalyzerError(
                                std::string("Unexpected error retaining DWARF module: ")
                                + dwfl_errmsg(dwfl_errno()));
                    }
                }
                return 0;
            },
            &callback_args);

    if (0 != rc) {
        throw ElfAnalyzerError(
                std::string("Unexpected error while filtering DWARF modules: ")
                + dwfl_errmsg(dwfl_errno()));
    }
}

void
CoreFileAnalyzer::resolveLibraries()
{
    struct RemappedModule
    {
        std::string modname;
        std::string path;
        GElf_Addr addr;
    };
    std::vector<RemappedModule> remapped_modules;

    LOG(DEBUG) << "Searching for missing and mismapped modules";
    removeModuleIf([this, &remapped_modules](Dwfl_Module* mod) -> bool {
        Dwarf_Addr start, end;
        const char* path;
        const char* modname =
                dwfl_module_info(mod, nullptr, &start, &end, nullptr, nullptr, &path, nullptr);
        if (!path) {
            path = modname;
        }

        std::string located_path;
        bool searched;
        if (!d_executable || !d_lib_search_path) {
            located_path = path;
            searched = false;
        } else {
            located_path = locateLibrary(path);
            searched = true;
        }
        bool const located_path_exists = fs::exists(located_path);

        if (!located_path_exists) {
            LOG(DEBUG) << "Adding " << path << " as a missing module "
                       << (searched ? "despite" : "without") << " a search";
            d_missing_modules.emplace_back(located_path);
        }

        if (located_path_exists && located_path != path) {
            std::string const filename = std::filesystem::path(located_path).filename().string();
            remapped_modules.push_back({filename, located_path, start});
            LOG(DEBUG) << "Dropping module " << path << " spanning from " << std::hex << std::showbase
                       << start << " to " << end << " so that it can be remapped from " << located_path;
            return true;
        } else {
            LOG(DEBUG) << "Retaining module " << path << " spanning from " << std::hex << std::showbase
                       << start << " to " << end;
            return false;
        }
    });

    LOG(DEBUG) << "Re-adding " << remapped_modules.size()
               << " mismapped modules with corrected locations";
    for (const auto& module : remapped_modules) {
        if (!dwfl_report_elf(
                    d_dwfl.get(),
                    module.modname.c_str(),
                    module.path.c_str(),
                    -1,
                    module.addr,
                    false))
        {
            LOG(ERROR) << "Failed to report module " << module.modname << ": "
                       << dwfl_errmsg(dwfl_errno());
            throw ElfAnalyzerError("Failed to report ELF modules for core file");
        } else {
            LOG(DEBUG) << "Reported module " << module.modname << " with path " << module.path
                       << " starting at " << std::hex << std::showbase << module.addr;
        }
    }

    LOG(DEBUG) << "Completing reporting of modules";
    if (dwfl_report_end(d_dwfl.get(), nullptr, nullptr) != 0) {
        throw ElfAnalyzerError(
                std::string("Unexpected error from dwfl_report_end: ") + dwfl_errmsg(dwfl_errno()));
    }
}

std::string
CoreFileAnalyzer::locateLibrary(const std::string& lib) const
{
    if (!d_lib_search_path) {
        return lib;
    }
    LOG(DEBUG) << "Searching for module: " << lib;
    std::string dir_to_consider;
    const fs::path target{lib};
    std::stringstream stream{d_lib_search_path.value()};
    while (std::getline(stream, dir_to_consider, ':')) {
        if (!fs::exists(dir_to_consider) || !fs::is_directory(dir_to_consider)) {
            continue;
        }
        for (const auto& entry : fs::directory_iterator(dir_to_consider)) {
            if (entry.path().filename() != target.filename()) {
                continue;
            }
            if (fs::is_regular_file(entry)) {
                LOG(DEBUG) << "Module " << lib << " found at " << entry.path().string();
                return entry.path().string();
            }
        }
    }
    LOG(DEBUG) << "Could not locate module " << lib << " in the search path";
    return lib;
}

ProcessAnalyzer::ProcessAnalyzer(pid_t pid)
: d_dwfl(nullptr)
, d_debuginfo_path(nullptr)
, d_callbacks()
, d_pid(pid)
{
    memset(&d_callbacks, 0, sizeof(d_callbacks));
    d_callbacks.find_elf = pystack_find_elf;
    d_callbacks.find_debuginfo = dwfl_standard_find_debuginfo;
    d_callbacks.debuginfo_path = &d_debuginfo_path;

    d_dwfl = dwfl_unique_ptr(dwfl_begin(&d_callbacks), dwfl_end);

    if (!d_dwfl) {
        throw ElfAnalyzerError("Failed to initialize DWARF analyzer");
    }

    if (dwfl_linux_proc_report(d_dwfl.get(), pid) || dwfl_report_end(d_dwfl.get(), nullptr, nullptr)) {
        throw ElfAnalyzerError("Failed to analyze DWARF information for the remote process");
    }

    if (dwfl_linux_proc_attach(d_dwfl.get(), pid, true) != 0) {
        throw ElfAnalyzerError("Could not attach the DWARF process analyzer");
    }
}

const dwfl_unique_ptr&
ProcessAnalyzer::getDwfl() const
{
    return d_dwfl;
}

static std::vector<NoteData>
getDataFromNoteSection(
        Elf* elf,
        Elf64_Word note_type,
        Elf_Type note_data_type,
        const GElf_Phdr* program_header,
        Elf_Data* data)
{
    size_t note_offset = 0;
    size_t name_offset = 0;
    size_t desc_offset = 0;
    GElf_Nhdr note_contents;
    auto is_note_of_desired_type = [](const char* name, Elf64_Word type, GElf_Nhdr nhdr) -> bool {
        return nhdr.n_type == type && (nhdr.n_namesz == 4 || (nhdr.n_namesz == 5 && name[4] == '\0'))
               && !::memcmp(name, "CORE", 4);
    };

    std::vector<NoteData> result;

    while (note_offset < data->d_size) {
        note_offset = gelf_getnote(data, note_offset, &note_contents, &name_offset, &desc_offset);
        if (note_offset <= 0) {
            break;
        }

        const char* note_name = note_contents.n_namesz == 0 ? "" : (char*)(data->d_buf) + name_offset;
        if (!is_note_of_desired_type(note_name, note_type, note_contents)) {
            LOG(DEBUG) << "Skipping NOTE segment with name " << note_name << " and type "
                       << note_contents.n_type;
            continue;
        }

        const GElf_Word descr_size = note_contents.n_descsz;
        const GElf_Off descr_location = program_header->p_offset + desc_offset;
        Elf_Data* note_data = elf_getdata_rawchunk(elf, descr_location, descr_size, note_data_type);
        if (note_data == nullptr) {
            LOG(WARNING) << "Invalid auxiliary NOTE data found in core file";
            // There may be some other note that has valid data, so we need to continue.
            continue;
        }

        LOG(DEBUG) << "Found NOTE of type " << note_type << " with name '" << note_name
                   << "' at position " << std::hex << std::showbase << descr_location;

        result.emplace_back(NoteData{elf, note_data, descr_size, desc_offset, note_contents});
    }
    if (result.empty()) {
        LOG(DEBUG) << "Failed to locate NOTE of type " << note_type << " in the core file";
    }
    return result;
}

std::vector<NoteData>
getNoteData(Elf* elf, Elf64_Word note_type, Elf_Type note_data_type)
{
    LOG(DEBUG) << "Searching for NOTE segments of type " << note_type;
    size_t n_program_headers;
    if (elf_getphdrnum(elf, &n_program_headers) < 0) {
        LOG(ERROR) << "Cannot determine number of program headers in the ELF file";
        return {};
    }

    // We have to look through the program header to find the note sections.
    // Note that there can be more than one.
    for (size_t program_header_idx = 0; program_header_idx < n_program_headers; ++program_header_idx) {
        GElf_Phdr mem;
        const GElf_Phdr* program_header = gelf_getphdr(elf, program_header_idx, &mem);

        if (program_header == nullptr || program_header->p_type != PT_NOTE) {
            continue;
        }
        LOG(DEBUG) << "Program header of type PT_NOTE found with offset " << std::hex << std::showbase
                   << program_header->p_offset;
        Elf_Data* data = elf_getdata_rawchunk(
                elf,
                program_header->p_offset,
                program_header->p_filesz,
                ELF_T_NHDR);

        if (data == nullptr) {
            LOG(WARNING) << "Invalid data in NOTE section at " << std::showbase << std::hex
                         << program_header->p_offset;
            continue;
        }

        LOG(DEBUG) << "Fetching data from NOTE segments of type " << note_type
                   << " in program header with offset " << std::hex << std::showbase
                   << program_header->p_offset;
        return getDataFromNoteSection(elf, note_type, note_data_type, program_header, data);
    }
    LOG(ERROR) << "Failed to locate a program header of type PT_NOTE in the core file";
    return {};
}

bool
getSectionInfo(const std::string& filename, const std::string& section_name, SectionInfo* result)
{
    if (elf_version(EV_CURRENT) == EV_NONE) {
        LOG(ERROR) << "libelf library ELF version too old";
        return false;
    }

    LOG(DEBUG) << "Trying to locate .PyRuntime data offset from program headers";
    file_unique_ptr file(fopen(filename.c_str(), "r"), fclose);
    if (!file || fileno(file.get()) == -1) {
        LOG(ERROR) << "Cannot open ELF file " << filename;
        return false;
    }
    const int fd = fileno(file.get());

    elf_unique_ptr elf = elf_unique_ptr(elf_begin(fd, ELF_C_READ_MMAP, nullptr), elf_end);
    if (!elf) {
        LOG(ERROR) << "Cannot read ELF file " << filename;
        return false;
    }

    Elf* the_elf = elf.get();

    size_t shnum;
    size_t nphdr;
    if (elf_getphdrnum(the_elf, &nphdr) != 0) {
        LOG(ERROR) << "Failed to get program headers";
        return false;
    }

    Dwarf_Addr load_point = 0;
    for (size_t i = 0; i < nphdr; i++) {
        GElf_Phdr phdr;
        if (gelf_getphdr(the_elf, i, &phdr) != &phdr) {
            continue;
        }

        if (phdr.p_type != PT_LOAD) {
            continue;
        }

        load_point = phdr.p_vaddr - phdr.p_vaddr % phdr.p_align;
        LOG(DEBUG) << "Found load point of main Python " << filename << " at " << std::hex
                   << std::showbase << load_point;
        break;
    }

    if (elf_getshdrnum(the_elf, &shnum) < 0) {
        LOG(ERROR) << "Cannot determine the number of sections in the ELF file";
        return false;
    }

    size_t shstrndx;
    if (elf_getshdrstrndx(the_elf, &shstrndx) < 0) {
        LOG(ERROR) << "Cannot get the section string table";
        return false;
    }

    LOG(DEBUG) << "Found " << shnum << " sections in the ELF file";

    LOG(DEBUG) << "Searching file " << filename << " for " << section_name << " section";

    if (shnum != 0) {
        Elf_Scn* scn = nullptr;
        while ((scn = elf_nextscn(the_elf, scn)) != nullptr) {
            GElf_Shdr shdr_mem;
            GElf_Shdr* shdr = gelf_getshdr(scn, &shdr_mem);
            if (shdr == nullptr) {
                continue;
            }
            const char* sname = elf_strptr(the_elf, shstrndx, shdr->sh_name) ?: "<corrupt>";
            LOG(DEBUG) << "Section found with name: " << sname;
            if (sname == nullptr || std::string(sname) != section_name) {
                continue;
            }
            LOG(DEBUG) << "Found " << section_name << " section with offset " << std::hex
                       << std::showbase << shdr->sh_addr;

            result->name = section_name;
            result->flags = parse_permissions(shdr->sh_flags);
            result->addr = shdr->sh_addr;
            result->corrected_addr = shdr->sh_addr - load_point;
            result->offset = shdr->sh_offset;
            result->size = shdr->sh_size;
            return true;
        }
    }
    return false;
}

const dwfl_unique_ptr&
CoreFileAnalyzer::getDwfl() const
{
    return d_dwfl;
}

static int
module_callback(
        Dwfl_Module* mod,
        void** userdata __attribute__((unused)),
        const char* name __attribute__((unused)),
        Dwarf_Addr starty __attribute__((unused)),
        void* arg)
{
    auto args = static_cast<std::pair<uintptr_t, const std::string&>*>(arg);
    if (args->first != 0) {
        return DWARF_CB_OK;
    }

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

    if (args->second == modname) {
        args->first = start;
        return DWARF_CB_ABORT;
    }

    return DWARF_CB_OK;
}

uintptr_t
getLoadPointOfModule(const dwfl_unique_ptr& dwfl, const std::string& mod)
{
    LOG(DEBUG) << "Finding load point of binary " << mod;
    auto args = std::pair<uintptr_t, const std::string&>(0, mod);
    if (dwfl_getmodules(dwfl.get(), module_callback, &args, 0) == -1) {
        LOG(ERROR) << "Failed to obtain load point of binary " << mod;
        return 0;
    }
    LOG(DEBUG) << "Load point of module found at " << std::hex << std::showbase << args.first;
    return args.first;
}

std::string
buildIdPtrToString(const uint8_t* id, ssize_t size)
{
    std::stringstream result;
    do {
        result << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(*id++);
    } while (--size > 0);
    return result.str();
}

std::string
getBuildId(const std::string& filename)
{
    if (elf_version(EV_CURRENT) == EV_NONE) {
        LOG(ERROR) << "libelf library ELF version too old";
        return "";
    }

    if (!fs::exists(filename)) {
        LOG(DEBUG) << filename << " does not exist";
        return "";
    }

    LOG(DEBUG) << "Trying to locate .PyRuntime data offset from program headers";
    file_unique_ptr file(fopen(filename.c_str(), "r"), fclose);
    if (!file || fileno(file.get()) == -1) {
        LOG(ERROR) << "Cannot open ELF file " << filename;
        return "";
    }
    const int fd = fileno(file.get());

    elf_unique_ptr elf = elf_unique_ptr(elf_begin(fd, ELF_C_READ_MMAP, nullptr), elf_end);
    if (!elf) {
        LOG(ERROR) << "Cannot read ELF file " << filename;
        return "";
    }

    Elf* the_elf = elf.get();

    const void* build_idp = nullptr;
    ssize_t elf_build_id_len = dwelf_elf_gnu_build_id(the_elf, &build_idp);
    if (elf_build_id_len <= 0) {
        return "";
    }
    return buildIdPtrToString(reinterpret_cast<const unsigned char*>(build_idp), elf_build_id_len);
}

}  // namespace pystack
