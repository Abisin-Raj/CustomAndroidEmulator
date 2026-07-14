#pragma once
#include "models/rom_inspection.h"
#include <filesystem>

class ImageParserEngine {
public:
    static RomInspection inspectRomDirectory(const std::filesystem::path& romDir);
};
