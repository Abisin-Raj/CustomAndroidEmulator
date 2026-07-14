#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct RomMetadata {
    uint32_t metadataVersion = 1;
    std::string name;
    std::string device = "Generic Device";
    std::string codename = "unknown";
    std::string androidVersion = "Unknown";
    std::string architecture = "arm64";
    std::string kernel = "Unknown";
    std::string partitionLayout = "A-only";
    std::vector<std::string> images;
    std::string avbStatus = "Unknown";
    std::string status = "Valid";
};
