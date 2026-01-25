#pragma once

#include <string>
#include <utility>

#include "maps_parser.h"
#include "mem.h"

namespace pystack {

using PythonVersion = std::pair<int, int>;

PythonVersion
getVersionForProcess(
        pid_t pid,
        const ProcessMemoryMapInfo& mapinfo,
        AbstractRemoteMemoryManager* manager);

PythonVersion
getVersionForCore(const std::string& corefile, const ProcessMemoryMapInfo& mapinfo);

}  // namespace pystack
