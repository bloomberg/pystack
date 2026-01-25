#pragma once

#include <fstream>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "corefile.h"
#include "elf_common.h"
#include "mem.h"

namespace pystack {

struct ProcessMemoryMapInfo
{
    std::optional<VirtualMap> heap;
    std::optional<VirtualMap> bss;
    VirtualMap python;
    std::optional<VirtualMap> libpython;
};

std::vector<VirtualMap>
parseProcMaps(pid_t pid);

std::vector<VirtualMap>
parseCoreFileMaps(
        const std::vector<CoreVirtualMap>& mapped_files,
        const std::vector<CoreVirtualMap>& memory_maps);

ProcessMemoryMapInfo
parseMapInformation(
        const std::string& binary,
        const std::vector<VirtualMap>& maps,
        const std::unordered_map<std::string, uintptr_t>* load_point_by_module = nullptr);

ProcessMemoryMapInfo
parseMapInformationForProcess(pid_t pid, const std::vector<VirtualMap>& maps);

std::optional<std::string>
getThreadName(pid_t pid, pid_t tid);

}  // namespace pystack
