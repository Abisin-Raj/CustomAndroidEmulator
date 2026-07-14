#include "image_parser/vbmeta_parser.h"
#include "core/utils.h"
#include "core/logger.h"
#include <fstream>
#include <vector>
#include <cstring>

VbmetaInfo VbmetaParser::parse(const std::filesystem::path& path) {
    VbmetaInfo info;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        LOG_PARSER_ERROR("Failed to open vbmeta file: {}", path.string());
        return info;
    }

    std::vector<char> header(256, 0);
    in.read(header.data(), header.size());
    if (in.gcount() < 256) {
        LOG_PARSER_WARN("VBMeta file too small to contain valid header (256 bytes)");
        return info;
    }

    if (std::memcmp(header.data(), "AVB0", 4) != 0) {
        LOG_PARSER_WARN("VBMeta file has invalid magic signature");
        return info;
    }

    info.isValid = true;

    uint64_t rollbackIndexBE = 0;
    std::memcpy(&rollbackIndexBE, header.data() + 112, sizeof(rollbackIndexBE));
    info.rollbackIndex = core::beToHost(rollbackIndexBE);

    uint32_t flagsBE = 0;
    std::memcpy(&flagsBE, header.data() + 120, sizeof(flagsBE));
    info.flags = core::beToHost(flagsBE);

    bool verificationDisabled = (info.flags & (1 << 1)) != 0;
    info.avbEnabled = !verificationDisabled;

    info.releaseString = std::string(header.data() + 128, strnlen(header.data() + 128, 48));

    LOG_PARSER_INFO("Parsed VBMeta Image: release='{}', avb_enabled={}, rollback_index={}, flags={:#x}",
        info.releaseString, info.avbEnabled, info.rollbackIndex, info.flags);

    return info;
}
