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
        LOG_ERROR("VbmetaParser: Failed to open file: {}", path.string());
        return info;
    }

    std::vector<char> header(256, 0);
    in.read(header.data(), header.size());
    if (in.gcount() < 256) {
        LOG_WARN("VbmetaParser: File too small to contain valid VBMeta header (256 bytes)");
        return info;
    }

    // Check magic: "AVB0"
    if (std::memcmp(header.data(), "AVB0", 4) != 0) {
        LOG_WARN("VbmetaParser: Invalid magic signature");
        return info;
    }

    info.isValid = true;

    // Read rollback index (offset 112, 8 bytes, uint64_t BE)
    uint64_t rollbackIndexBE = 0;
    std::memcpy(&rollbackIndexBE, header.data() + 112, sizeof(rollbackIndexBE));
    info.rollbackIndex = core::beToHost(rollbackIndexBE);

    // Read flags (offset 120, 4 bytes, uint32_t BE)
    uint32_t flagsBE = 0;
    std::memcpy(&flagsBE, header.data() + 120, sizeof(flagsBE));
    info.flags = core::beToHost(flagsBE);

    // AVB_VBMETA_IMAGE_FLAGS_VERIFICATION_DISABLED = 1 << 1
    // If the bit 1 is set, verification is disabled
    bool verificationDisabled = (info.flags & (1 << 1)) != 0;
    info.avbEnabled = !verificationDisabled;

    // Read release string (offset 128, 48 bytes)
    info.releaseString = std::string(header.data() + 128, strnlen(header.data() + 128, 48));

    LOG_INFO("Parsed VBMeta Image: release='{}', avb_enabled={}, rollback_index={}, flags={:#x}",
        info.releaseString, info.avbEnabled, info.rollbackIndex, info.flags);

    return info;
}
