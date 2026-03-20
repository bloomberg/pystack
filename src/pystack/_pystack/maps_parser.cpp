#include "maps_parser.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

#include "logging.h"

namespace pystack {

namespace fs = std::filesystem;

// Regex pattern for parsing /proc/pid/maps lines
// Format: start-end permissions offset dev inode pathname
static const std::regex MAPS_REGEXP(
        R"(([0-9a-f]+)-([0-9a-f]+)\s+(.{4})\s+([0-9a-f]+)\s+([0-9a-f]+:[0-9a-f]+)\s+(\d+)\s*(.*)?)");

std::vector<VirtualMap>
parseProcMaps(pid_t pid)
{
    std::vector<VirtualMap> maps;
    std::string maps_path = "/proc/" + std::to_string(pid) + "/maps";

    std::ifstream maps_file(maps_path);
    if (!maps_file.is_open()) {
        throw std::runtime_error("No such process id: " + std::to_string(pid));
    }

    std::string line;
    while (std::getline(maps_file, line)) {
        std::smatch match;
        if (!std::regex_match(line, match, MAPS_REGEXP)) {
            LOG(DEBUG) << "Line cannot be recognized: " << line;
            continue;
        }

        uintptr_t start = std::stoull(match[1].str(), nullptr, 16);
        uintptr_t end = std::stoull(match[2].str(), nullptr, 16);
        std::string permissions = match[3].str();
        unsigned long offset = std::stoul(match[4].str(), nullptr, 16);
        std::string device = match[5].str();
        unsigned long inode = std::stoul(match[6].str());
        std::string pathname = match[7].str();

        size_t start_pos = pathname.find_first_not_of(" \t");
        if (start_pos != std::string::npos) {
            pathname = pathname.substr(start_pos);
        } else {
            pathname = "";
        }

        maps.emplace_back(
                start,
                end,
                end - start,  // filesize
                permissions,
                offset,
                device,
                inode,
                pathname);
    }

    return maps;
}

std::vector<VirtualMap>
parseCoreFileMaps(
        const std::vector<CoreVirtualMap>& mapped_files,
        const std::vector<CoreVirtualMap>& memory_maps)
{
    std::set<std::pair<uintptr_t, uintptr_t>> memory_map_ranges;
    for (const auto& map : memory_maps) {
        memory_map_ranges.insert({map.start, map.end});
    }

    std::vector<CoreVirtualMap> missing_mapped_files;
    for (const auto& map : mapped_files) {
        if (memory_map_ranges.find({map.start, map.end}) == memory_map_ranges.end()) {
            missing_mapped_files.push_back(map);
        }
    }

    std::vector<CoreVirtualMap> all_maps;
    all_maps.reserve(memory_maps.size() + missing_mapped_files.size());
    all_maps.insert(all_maps.end(), memory_maps.begin(), memory_maps.end());
    all_maps.insert(all_maps.end(), missing_mapped_files.begin(), missing_mapped_files.end());

    std::sort(all_maps.begin(), all_maps.end(), [](const CoreVirtualMap& a, const CoreVirtualMap& b) {
        return a.start < b.start;
    });

    std::set<std::string> missing_map_paths;
    for (const auto& map : missing_mapped_files) {
        if (!map.path.empty()) {
            try {
                missing_map_paths.insert(fs::canonical(map.path).string());
            } catch (...) {
                missing_map_paths.insert(map.path);
            }
        }
    }

    std::unordered_map<std::string, std::string> file_maps;
    for (const auto& map : memory_maps) {
        if (map.path.empty()) {
            continue;
        }
        try {
            std::string resolved_path = fs::canonical(map.path).string();
            if (missing_map_paths.count(resolved_path)) {
                file_maps[resolved_path] = map.path;
            }
        } catch (...) {
            // Ignore errors resolving paths
        }
    }

    std::vector<VirtualMap> result;
    result.reserve(all_maps.size());
    for (const auto& elem : all_maps) {
        std::string path = elem.path;
        if (!path.empty()) {
            std::string resolved;
            try {
                resolved = fs::canonical(path).string();
            } catch (...) {
                resolved = path;
            }
            auto it = file_maps.find(resolved);
            if (it != file_maps.end()) {
                path = it->second;
            }
        }
        result.emplace_back(
                elem.start,
                elem.end,
                elem.filesize,
                elem.flags,
                elem.offset,
                elem.device,
                elem.inode,
                path);
    }

    return result;
}

static VirtualMap
getBaseMap(const std::vector<VirtualMap>& binary_maps)
{
    for (const auto& map : binary_maps) {
        if (!map.Path().empty()) {
            return map;
        }
    }
    if (!binary_maps.empty()) {
        return binary_maps[0];
    }
    throw std::runtime_error("No maps available");
}

static std::optional<VirtualMap>
getBss(const std::vector<VirtualMap>& elf_maps, uintptr_t load_point)
{
    if (elf_maps.empty()) {
        return std::nullopt;
    }

    VirtualMap binary_map = getBaseMap(elf_maps);
    if (binary_map.Path().empty()) {
        return std::nullopt;
    }

    SectionInfo bss_info;
    if (!getSectionInfo(binary_map.Path(), ".bss", &bss_info)) {
        return std::nullopt;
    }

    uintptr_t start = load_point + bss_info.corrected_addr;
    LOG(INFO) << "Determined exact addr of .bss section: " << std::hex << start << " (" << load_point
              << " + " << bss_info.corrected_addr << ")" << std::dec;

    unsigned long offset = 0;

    const VirtualMap* first_matching_map = nullptr;
    for (const auto& map : elf_maps) {
        if (map.containsAddr(start)) {
            first_matching_map = &map;
            break;
        }
    }

    if (!first_matching_map) {
        return std::nullopt;
    }

    offset = first_matching_map->Offset() + (start - first_matching_map->Start());

    return VirtualMap(
            start,
            start + bss_info.size,
            bss_info.size,
            "",  // flags
            offset,  // offset
            "",  // device
            0,  // inode
            "");  // path
}

ProcessMemoryMapInfo
parseMapInformation(
        const std::string& binary,
        const std::vector<VirtualMap>& maps,
        const std::unordered_map<std::string, uintptr_t>* load_point_by_module)
{
    std::unordered_map<std::string, std::vector<VirtualMap>> maps_by_library;
    std::string current_lib;

    std::unordered_map<std::string, uintptr_t> computed_load_points;
    if (!load_point_by_module) {
        for (const auto& map : maps) {
            if (!map.Path().empty()) {
                std::string name = fs::path(map.Path()).filename().string();
                if (computed_load_points.find(name) == computed_load_points.end()) {
                    computed_load_points[name] = map.Start();
                } else {
                    computed_load_points[name] = std::min(computed_load_points[name], map.Start());
                }
            }
        }
        load_point_by_module = &computed_load_points;
    }

    for (const auto& memory_range : maps) {
        std::string path_name;
        if (!memory_range.Path().empty()) {
            path_name = fs::path(memory_range.Path()).filename().string();
            current_lib = path_name;
        } else {
            path_name = current_lib;
        }
        maps_by_library[path_name].push_back(memory_range);
    }

    std::string binary_name = fs::path(binary).filename().string();

    auto python_it = maps_by_library.find(binary_name);
    if (python_it == maps_by_library.end()) {
        // Construct error message with available maps
        std::ostringstream available;
        for (const auto& map : maps) {
            if (!map.Path().empty() && map.Path().find(".so") == std::string::npos) {
                available << map.Path() << ", ";
            }
        }
        std::string available_str = available.str();
        if (available_str.length() >= 2) {
            available_str = available_str.substr(0, available_str.length() - 2);
        }
        throw std::runtime_error(
                "Unable to find maps for the executable " + binary
                + ". Available executable maps: " + available_str);
    }

    const std::vector<VirtualMap>& binary_maps = python_it->second;
    VirtualMap python = getBaseMap(binary_maps);
    LOG(INFO) << "python binary first map found: " << python.Path();

    std::optional<VirtualMap> libpython;
    const std::vector<VirtualMap>* elf_maps = nullptr;
    std::string libpython_name;

    std::vector<std::string> libpython_binaries;
    for (const auto& [lib_name, _] : maps_by_library) {
        if (lib_name.find("libpython") != std::string::npos) {
            libpython_binaries.push_back(lib_name);
        }
    }

    uintptr_t load_point = 0;
    if (libpython_binaries.size() > 1) {
        throw std::runtime_error(
                "Unexpectedly found multiple libpython in process: "
                + std::to_string(libpython_binaries.size()));
    } else if (libpython_binaries.size() == 1) {
        libpython_name = libpython_binaries[0];
        const auto& libpython_maps = maps_by_library[libpython_name];
        elf_maps = &libpython_maps;
        auto load_it = load_point_by_module->find(libpython_name);
        load_point = (load_it != load_point_by_module->end()) ? load_it->second : UINTPTR_MAX;
        libpython = getBaseMap(libpython_maps);
        LOG(INFO) << libpython_name << " first map found: " << libpython->Path();
    } else {
        LOG(INFO) << "Process does not have a libpython.so, reading from binary";
        elf_maps = &binary_maps;
        auto load_it = load_point_by_module->find(binary_name);
        load_point = (load_it != load_point_by_module->end()) ? load_it->second : UINTPTR_MAX;
    }

    std::optional<VirtualMap> heap;
    auto heap_it = maps_by_library.find("[heap]");
    if (heap_it != maps_by_library.end() && !heap_it->second.empty()) {
        heap = heap_it->second.front();
        LOG(INFO) << "Heap map found";
    }

    std::optional<VirtualMap> bss = getBss(*elf_maps, load_point);
    if (!bss) {
        for (const auto& map : *elf_maps) {
            if (map.Path().empty() && map.Flags().find('r') != std::string::npos) {
                bss = map;
                break;
            }
        }
    }
    if (bss) {
        LOG(INFO) << "bss map found";
    }

    return ProcessMemoryMapInfo{heap, bss, python, libpython};
}

ProcessMemoryMapInfo
parseMapInformationForProcess(pid_t pid, const std::vector<VirtualMap>& maps)
{
    std::string exe_link = "/proc/" + std::to_string(pid) + "/exe";
    char exe_path[PATH_MAX];
    ssize_t len = readlink(exe_link.c_str(), exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        throw std::runtime_error("Failed to read /proc/" + std::to_string(pid) + "/exe");
    }
    exe_path[len] = '\0';
    return parseMapInformation(exe_path, maps);
}

std::optional<std::string>
getThreadName(pid_t pid, pid_t tid)
{
    std::string comm_path = "/proc/" + std::to_string(pid) + "/task/" + std::to_string(tid) + "/comm";
    std::ifstream comm_file(comm_path);
    if (!comm_file.is_open()) {
        return std::nullopt;
    }

    std::string name;
    std::getline(comm_file, name);

    size_t end = name.find_last_not_of(" \t\n\r");
    if (end != std::string::npos) {
        name = name.substr(0, end + 1);
    }

    return name;
}

}  // namespace pystack
