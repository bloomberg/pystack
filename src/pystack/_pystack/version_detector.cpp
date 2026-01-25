#include "version_detector.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <vector>

#include "logging.h"

namespace pystack {

namespace fs = std::filesystem;

// Regex patterns for version detection
// Matches: "3.8.10 (default, May 26 2023, 14:05:08)" or similar version strings in BSS
static const std::regex BSS_VERSION_REGEXP(
        R"(((2|3)\.(\d+)\.(\d{1,2}))((a|b|c|rc)\d{1,2})?\+?(?: (?:experimental )?free-threading build)? (\(.{1,64}\)))");

// Matches: python3.8, python3.10, etc.
static const std::regex BINARY_REGEXP(R"(python(\d+)\.(\d+).*)", std::regex_constants::icase);

// Matches: libpython3.8.so, libpython3.10.so.1.0, etc.
static const std::regex LIBPYTHON_REGEXP(R"(.*libpython(\d+)\.(\d+).*)", std::regex_constants::icase);

static std::optional<PythonVersion>
scanProcessBssForVersion(pid_t pid, const VirtualMap& bss, AbstractRemoteMemoryManager* manager)
{
    if (!manager) {
        return std::nullopt;
    }

    size_t size = bss.Size();
    std::vector<char> memory(size);

    try {
        ssize_t bytes_read = manager->copyMemoryFromProcess(bss.Start(), size, memory.data());
        if (bytes_read < 0) {
            return std::nullopt;
        }
    } catch (...) {
        return std::nullopt;
    }

    std::string memory_str(memory.begin(), memory.end());
    std::smatch match;
    if (std::regex_search(memory_str, match, BSS_VERSION_REGEXP)) {
        int major = std::stoi(match[2].str());
        int minor = std::stoi(match[3].str());
        return PythonVersion(major, minor);
    }

    return std::nullopt;
}

static std::optional<PythonVersion>
scanCoreBssForVersion(const std::string& corefile, const VirtualMap& bss)
{
    std::ifstream file(corefile, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    file.seekg(bss.Offset());
    if (!file.good()) {
        return std::nullopt;
    }

    size_t size = bss.Size();
    std::vector<char> data(size);
    file.read(data.data(), size);
    if (!file.good() && !file.eof()) {
        return std::nullopt;
    }

    std::string data_str(data.begin(), data.end());
    std::smatch match;
    if (std::regex_search(data_str, match, BSS_VERSION_REGEXP)) {
        int major = std::stoi(match[2].str());
        int minor = std::stoi(match[3].str());
        return PythonVersion(major, minor);
    }

    return std::nullopt;
}

static std::optional<PythonVersion>
inferVersionFromPath(const std::string& path)
{
    std::string filename = fs::path(path).filename().string();

    std::smatch match;
    if (std::regex_match(filename, match, LIBPYTHON_REGEXP)) {
        int major = std::stoi(match[1].str());
        int minor = std::stoi(match[2].str());
        LOG(INFO) << "Version inferred from libpython path: " << major << "." << minor;
        return PythonVersion(major, minor);
    }

    if (std::regex_match(filename, match, BINARY_REGEXP)) {
        int major = std::stoi(match[1].str());
        int minor = std::stoi(match[2].str());
        LOG(INFO) << "Version inferred from binary path: " << major << "." << minor;
        return PythonVersion(major, minor);
    }

    return std::nullopt;
}

// Matches: "Python 3.10.4" or similar --version output
static const std::regex VERSION_OUTPUT_REGEXP(R"(Python (\d+)\.(\d+).*)", std::regex_constants::icase);

static std::optional<PythonVersion>
getVersionFromBinary(const std::string& binary_path)
{
    std::string cmd = binary_path + " --version 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return std::nullopt;
    }

    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    pclose(pipe);

    std::smatch match;
    if (std::regex_search(output, match, VERSION_OUTPUT_REGEXP)) {
        int major = std::stoi(match[1].str());
        int minor = std::stoi(match[2].str());
        LOG(INFO) << "Version found by running --version: " << major << "." << minor;
        return PythonVersion(major, minor);
    }

    return std::nullopt;
}

static PythonVersion
getVersionFromMapInfo(const ProcessMemoryMapInfo& mapinfo)
{
    if (mapinfo.libpython && !mapinfo.libpython->Path().empty()) {
        LOG(INFO) << "Trying to extract version from filename: " << mapinfo.libpython->Path();
        auto version = inferVersionFromPath(mapinfo.libpython->Path());
        if (version) {
            return *version;
        }
    }

    if (!mapinfo.python.Path().empty()) {
        LOG(INFO) << "Trying to extract version from filename: " << mapinfo.python.Path();
        auto version = inferVersionFromPath(mapinfo.python.Path());
        if (version) {
            return *version;
        }

        LOG(INFO) << "Could not find version by looking at library or binary path: "
                     "Trying to get it from running python --version";
        auto bin_version = getVersionFromBinary(mapinfo.python.Path());
        if (bin_version) {
            return *bin_version;
        }
    }

    throw std::runtime_error("Could not determine python version from " + mapinfo.python.Path());
}

PythonVersion
getVersionForProcess(
        pid_t pid,
        const ProcessMemoryMapInfo& mapinfo,
        AbstractRemoteMemoryManager* manager)
{
    if (mapinfo.bss) {
        auto version = scanProcessBssForVersion(pid, *mapinfo.bss, manager);
        if (version) {
            LOG(INFO) << "Version found by scanning the bss section: " << version->first << "."
                      << version->second;
            return *version;
        }
    }

    return getVersionFromMapInfo(mapinfo);
}

PythonVersion
getVersionForCore(const std::string& corefile, const ProcessMemoryMapInfo& mapinfo)
{
    if (mapinfo.bss) {
        auto version = scanCoreBssForVersion(corefile, *mapinfo.bss);
        if (version) {
            LOG(INFO) << "Version found by scanning the bss section: " << version->first << "."
                      << version->second;
            return *version;
        }
    }

    return getVersionFromMapInfo(mapinfo);
}

}  // namespace pystack
