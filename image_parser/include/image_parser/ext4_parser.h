#pragma once
#include <filesystem>

class Ext4Parser {
public:
    static bool isExt4Image(const std::filesystem::path& path);
};
