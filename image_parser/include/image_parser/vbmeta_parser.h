#pragma once
#include "models/vbmeta_info.h"
#include <filesystem>

class VbmetaParser {
public:
    static VbmetaInfo parse(const std::filesystem::path& path);
};
