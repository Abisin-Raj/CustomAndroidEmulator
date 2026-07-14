#pragma once
#include <filesystem>

class SuperParser {
public:
    static bool isSuperImage(const std::filesystem::path& path);
};
