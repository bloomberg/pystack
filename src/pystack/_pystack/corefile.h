#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "elf_common.h"
#include "mem.h"

namespace pystack {

struct CoreCrashInfo
{
    int si_signo;
    int si_errno;
    int si_code;
    int sender_pid;
    int sender_uid;
    uintptr_t failed_addr;
};

static const size_t FNAME_SIZE = 16;
static const size_t PSARGS_SIZE = 80;

struct CorePsInfo
{
    char state;
    char sname;
    char zomb;
    char nice;
    ulong flag;
    int uid;
    int gid;
    pid_t pid;
    pid_t ppid;
    pid_t pgrp;
    pid_t sid;
    char fname[FNAME_SIZE];
    char psargs[PSARGS_SIZE];
};

class CoreAnalyzerError : public std::exception
{
  public:
    explicit CoreAnalyzerError(std::string error)
    : d_error(std::move(error)){};

    [[nodiscard]] const char* what() const noexcept override
    {
        return d_error.c_str();
    }

  private:
    std::string d_error;
};

// This is a struct so cython can convert easily from this
struct CoreVirtualMap
{
    // Data members
    uintptr_t start;
    uintptr_t end;
    unsigned long filesize;
    std::string flags;
    unsigned long offset;
    std::string device;
    unsigned long inode;
    std::string path;
    std::string buildid;
};

class CoreFileExtractor
{
  public:
    // Constructors
    explicit CoreFileExtractor(std::shared_ptr<CoreFileAnalyzer> analyzer);

    // Methods
    std::vector<CoreVirtualMap> MemoryMaps() const;
    std::vector<SimpleVirtualMap> ModuleInformation() const;
    pid_t Pid() const;
    std::string extractExecutable() const;
    CoreCrashInfo extractFailureInfo() const;
    CorePsInfo extractPSInfo() const;
    const std::vector<CoreVirtualMap> extractMappedFiles() const;
    std::vector<std::string> missingModules() const;

  private:
    // Data members
    std::shared_ptr<CoreFileAnalyzer> d_analyzer;
    std::vector<SimpleVirtualMap> d_module_info;
    std::vector<CoreVirtualMap> d_maps;

    // Methods
    void populateMaps();
    uintptr_t findExecFn() const;
};
}  // namespace pystack
