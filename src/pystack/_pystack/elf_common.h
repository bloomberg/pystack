#pragma once

#include <cstring>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unistd.h>

#include "logging.h"
#include <elf.h>
#include <elfutils/libdwelf.h>
#include <elfutils/libdwfl.h>
#include <gelf.h>

namespace pystack {

std::string
parse_permissions(long flags);

class ElfAnalyzerError : public std::exception
{
  public:
    explicit ElfAnalyzerError(std::string error)
    : d_error(std::move(error)){};

    const char* what() const noexcept override
    {
        return d_error.c_str();
    }

  private:
    std::string d_error;
};

// Aliases
using dwfl_unique_ptr = std::unique_ptr<Dwfl, std::function<void(Dwfl*)>>;
using elf_unique_ptr = std::unique_ptr<Elf, std::function<void(Elf*)>>;

class Analyzer
{
    // Methods
  public:
    virtual const dwfl_unique_ptr& getDwfl() const = 0;
};

class CoreFileAnalyzer : public Analyzer
{
  public:
    // Constructors
    explicit CoreFileAnalyzer(
            std::string corefile,
            std::optional<std::string> executable = std::nullopt,
            const std::optional<std::string>& lib_search_path = std::nullopt);

    // Methods
    const dwfl_unique_ptr& getDwfl() const override;
    std::string locateLibrary(const std::string& lib) const;

    // Destructors
    ~CoreFileAnalyzer();

    // Data members
    dwfl_unique_ptr d_dwfl;
    char* d_debuginfo_path;
    Dwfl_Callbacks d_callbacks;
    std::string d_filename;
    std::optional<std::string> d_executable;
    std::optional<std::string> d_lib_search_path;
    int d_fd;
    int d_pid;
    elf_unique_ptr d_elf;
    std::vector<std::string> d_missing_modules{};

  private:
    void removeModuleIf(std::function<bool(Dwfl_Module*)> predicate) const;
    void resolveLibraries();
};

class ProcessAnalyzer : public Analyzer
{
  public:
    // Constructors
    explicit ProcessAnalyzer(pid_t pid);

    // Methods
    const dwfl_unique_ptr& getDwfl() const override;

    // Data members
    dwfl_unique_ptr d_dwfl;
    char* d_debuginfo_path;
    Dwfl_Callbacks d_callbacks;
    int d_pid;
};

uintptr_t
getLoadPointOfModule(const dwfl_unique_ptr& dwfl, const std::string& mod);

// Utility functions for accessing NOTE sections

struct NoteData
{
    Elf* elf{nullptr};
    Elf_Data* data{nullptr};
    Elf64_Xword descriptor_size{0};
    size_t desc_offset{0};
    GElf_Nhdr nhdr{};
};

std::vector<NoteData>
getNoteData(Elf* elf, Elf64_Word note_type, Elf_Type note_data_type);

struct SectionInfo
{
    std::string name;
    std::string flags;
    uintptr_t addr;
    uintptr_t corrected_addr;
    off_t offset;
    size_t size;
};

bool
getSectionInfo(const std::string& filename, const std::string& section_name, SectionInfo* result);

std::string
buildIdPtrToString(const uint8_t* id, ssize_t size);

std::string
getBuildId(const std::string& filename);

}  // namespace pystack
