#pragma once
#include <string>
#include <filesystem>

struct VbmetaInfo {
    bool isValid = false;
    bool avbEnabled = false;
    std::string releaseString;
    uint32_t flags = 0;
    uint64_t rollbackIndex = 0;
};

class VbmetaParser {
public:
    static VbmetaInfo parse(const std::filesystem::path& path);
};
