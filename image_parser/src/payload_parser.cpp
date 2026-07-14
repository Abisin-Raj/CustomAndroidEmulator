#include "image_parser/payload_parser.h"
#include "core/utils.h"
#include "core/logger.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <numeric>

PayloadInfo PayloadParser::parse(const std::filesystem::path& path) {
    PayloadInfo info;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        LOG_PARSER_ERROR("Failed to open payload file: {}", path.string());
        return info;
    }

    std::vector<char> header(24, 0);
    in.read(header.data(), header.size());
    if (in.gcount() < 24) {
        LOG_PARSER_WARN("Payload file too small to contain Delta Archive Header");
        return info;
    }

    if (std::memcmp(header.data(), "CrAU", 4) != 0) {
        LOG_PARSER_WARN("Payload file has invalid magic signature");
        return info;
    }

    info.isValid = true;

    uint64_t versionBE = 0;
    std::memcpy(&versionBE, header.data() + 4, sizeof(versionBE));
    info.version = core::beToHost(versionBE);

    uint64_t manifestSizeBE = 0;
    std::memcpy(&manifestSizeBE, header.data() + 12, sizeof(manifestSizeBE));
    info.manifestSize = core::beToHost(manifestSizeBE);

    LOG_PARSER_INFO("Parsed Payload Header: version={}, manifest_size={}", info.version, info.manifestSize);

    if (info.manifestSize > 0 && info.manifestSize < 50 * 1024 * 1024) {
        std::vector<char> manifest(info.manifestSize, 0);
        in.read(manifest.data(), info.manifestSize);
        if (static_cast<uint64_t>(in.gcount()) == info.manifestSize) {
            std::string_view view(manifest.data(), info.manifestSize);

            const std::vector<std::string> candidates = {
                "boot", "system", "vendor", "product", "odm", 
                "system_ext", "vbmeta", "dtbo", "recovery", "vendor_boot",
                "super", "userdata", "metadata", "cust", "oem", "persist"
            };

            for (const auto& cand : candidates) {
                size_t pos = 0;
                while ((pos = view.find(cand, pos)) != std::string_view::npos) {
                    if (std::find(info.partitions.begin(), info.partitions.end(), cand) == info.partitions.end()) {
                        info.partitions.push_back(cand);
                    }
                    pos += cand.length();
                }
            }
        } else {
            LOG_PARSER_WARN("Failed to read complete manifest block of size {}", info.manifestSize);
        }
    }

    LOG_PARSER_INFO("Payload contains {} partitions: [{}]", 
        info.partitions.size(), 
        info.partitions.empty() ? "" : std::accumulate(info.partitions.begin() + 1, info.partitions.end(), info.partitions[0],
            [](const std::string& a, const std::string& b) { return a + ", " + b; })
    );

    return info;
}
