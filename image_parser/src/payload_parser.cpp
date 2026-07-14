#include "image_parser/payload_parser.h"
#include "core/utils.h"
#include "core/logger.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <algorithm>

PayloadInfo PayloadParser::parse(const std::filesystem::path& path) {
    PayloadInfo info;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        LOG_ERROR("PayloadParser: Failed to open file: {}", path.string());
        return info;
    }

    // Read Delta Archive Header (24 bytes)
    std::vector<char> header(24, 0);
    in.read(header.data(), header.size());
    if (in.gcount() < 24) {
        LOG_WARN("PayloadParser: File too small to contain Delta Archive Header");
        return info;
    }

    // Check magic: "CrAU"
    if (std::memcmp(header.data(), "CrAU", 4) != 0) {
        LOG_WARN("PayloadParser: Invalid magic signature");
        return info;
    }

    info.isValid = true;

    // Read format version (offset 4, uint64_t BE)
    uint64_t versionBE = 0;
    std::memcpy(&versionBE, header.data() + 4, sizeof(versionBE));
    info.version = core::beToHost(versionBE);

    // Read manifest size (offset 12, uint64_t BE)
    uint64_t manifestSizeBE = 0;
    std::memcpy(&manifestSizeBE, header.data() + 12, sizeof(manifestSizeBE));
    info.manifestSize = core::beToHost(manifestSizeBE);

    LOG_INFO("Parsed Payload Header: version={}, manifest_size={}", info.version, info.manifestSize);

    // Scan manifest block for partition names
    if (info.manifestSize > 0 && info.manifestSize < 50 * 1024 * 1024) { // Max 50MB sanity check
        std::vector<char> manifest(info.manifestSize, 0);
        in.read(manifest.data(), info.manifestSize);
        if (static_cast<uint64_t>(in.gcount()) == info.manifestSize) {
            std::string_view view(manifest.data(), info.manifestSize);

            // A list of common Android partitions to look for inside protobuf manifest
            const std::vector<std::string> candidates = {
                "boot", "system", "vendor", "product", "odm", 
                "system_ext", "vbmeta", "dtbo", "recovery", "vendor_boot",
                "super", "userdata", "metadata", "cust", "oem", "persist"
            };

            for (const auto& cand : candidates) {
                // Protobuf format stores string field partition_name.
                // We do a simple find of candidate partition names
                size_t pos = 0;
                while ((pos = view.find(cand, pos)) != std::string_view::npos) {
                    // Make sure it's a separate token/match (e.g. check surrounding characters or just add it)
                    // If we match exactly, add it to partitions list
                    if (std::find(info.partitions.begin(), info.partitions.end(), cand) == info.partitions.end()) {
                        info.partitions.push_back(cand);
                    }
                    pos += cand.length();
                }
            }
        } else {
            LOG_WARN("PayloadParser: Failed to read complete manifest block of size {}", info.manifestSize);
        }
    }

    LOG_INFO("Payload contains {} partitions: [{}]", 
        info.partitions.size(), 
        info.partitions.empty() ? "" : std::accumulate(info.partitions.begin() + 1, info.partitions.end(), info.partitions[0],
            [](const std::string& a, const std::string& b) { return a + ", " + b; })
    );

    return info;
}
