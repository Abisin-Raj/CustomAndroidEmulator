#pragma once
#include "models/payload_info.h"
#include <filesystem>

class PayloadParser {
public:
    static PayloadInfo parse(const std::filesystem::path& path);
};
