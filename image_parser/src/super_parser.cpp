#include "image_parser/super_parser.h"
#include "core/logger.h"
#include <fstream>
#include <vector>
#include <cstring>

bool SuperParser::isSuperImage(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        LOG_ERROR("SuperParser: Failed to open file: {}", path.string());
        return false;
    }

    // Read first 16KB to look for LP Metadata magic (located at offset 4096 or offset 0)
    std::vector<char> buffer(16384, 0);
    in.read(buffer.data(), buffer.size());
    size_t bytesRead = in.gcount();
    if (bytesRead < 4) return false;

    // LP_METADATA_GEOMETRY_MAGIC = 0x616c4467 ("gDl\x61")
    // LP_METADATA_HEADER_MAGIC   = 0x414C5030 ("0PL\x41")
    // Check both endian versions
    for (size_t i = 0; i <= bytesRead - 4; ++i) {
        uint32_t val = 0;
        std::memcpy(&val, buffer.data() + i, 4);
        if (val == 0x616c4467 || val == 0x414C5030 || val == 0x67446c61 || val == 0x30504C41) {
            LOG_INFO("SuperParser: Detected super partition signatures at byte offset {}", i);
            return true;
        }
    }

    return false;
}
