#pragma once
#include "models/boot_info.h"
#include <filesystem>

class BootImgParser {
public:
    static BootImgInfo parse(const std::filesystem::path& path);
};
