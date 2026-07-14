#pragma once
#include <string>
#include <vector>
#include <filesystem>

struct PayloadInfo {
    bool isValid = false;
    uint64_t version = 0;
    uint64_t manifestSize = 0;
    std::vector<std::string> partitions;
};

class PayloadParser {
public:
    static PayloadInfo parse(const std::filesystem::path& path);
};
