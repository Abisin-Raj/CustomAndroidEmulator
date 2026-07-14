#pragma once
#include <string>
#include <vector>

struct RomInspection {
    std::string kernelRelease = "Unknown";
    std::string osVersion = "Unknown";
    std::string osPatchLevel = "Unknown";
    std::string partitionLayout = "A-only";
    bool avbEnabled = false;
    std::string avbRelease = "Unknown";
    std::vector<std::string> images;
    std::vector<std::string> logicalPartitions;
};
