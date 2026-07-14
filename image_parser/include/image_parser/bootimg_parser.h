#pragma once
#include <string>
#include <filesystem>

struct BootImgInfo {
    bool isValid = false;
    uint32_t headerVersion = 0;
    uint32_t pageSize = 0;
    uint64_t kernelSize = 0;
    uint64_t ramdiskSize = 0;
    std::string osVersion = "Unknown";
    std::string osPatchLevel = "Unknown";
    std::string cmdline;
    std::string kernelRelease = "Unknown";
};

class BootImgParser {
public:
    static BootImgInfo parse(const std::filesystem::path& path);
};
