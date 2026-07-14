#pragma once
#include <string>
#include <cstdint>

struct VbmetaInfo {
    bool isValid = false;
    bool avbEnabled = false;
    std::string releaseString;
    uint32_t flags = 0;
    uint64_t rollbackIndex = 0;
};
