#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct PayloadInfo {
    bool isValid = false;
    uint64_t version = 0;
    uint64_t manifestSize = 0;
    std::vector<std::string> partitions;
};
