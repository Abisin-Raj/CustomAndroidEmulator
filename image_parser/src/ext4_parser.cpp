#include "image_parser/ext4_parser.h"
#include "core/logger.h"
#include <fstream>
#include <vector>
#include <cstring>

bool Ext4Parser::isExt4Image(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        LOG_PARSER_ERROR("Failed to open file for ext4 check: {}", path.string());
        return false;
    }

    uint32_t first4Bytes = 0;
    in.read(reinterpret_cast<char*>(&first4Bytes), sizeof(first4Bytes));
    if (in.gcount() == 4) {
        if (first4Bytes == 0xED26FF3A) {
            LOG_PARSER_INFO("Detected Android Sparse File magic: {}", path.filename().string());
            return true;
        }
    }

    in.seekg(1080);
    uint16_t ext4Magic = 0;
    in.read(reinterpret_cast<char*>(&ext4Magic), sizeof(ext4Magic));
    if (in.gcount() == 2) {
        if (ext4Magic == 0xEF53) {
            LOG_PARSER_INFO("Detected raw EXT4 filesystem magic: {}", path.filename().string());
            return true;
        }
    }

    return false;
}
