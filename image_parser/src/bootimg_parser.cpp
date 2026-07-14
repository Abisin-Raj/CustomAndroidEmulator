#include "image_parser/bootimg_parser.h"
#include "core/logger.h"
#include <fstream>
#include <vector>
#include <cstring>

namespace {

void decodeOsVersion(uint32_t osVal, std::string& osVersion, std::string& osPatchLevel) {
    if (osVal == 0) return;

    // osVal bitfields:
    // 31-25: major version (7 bits)
    // 24-18: minor version (7 bits)
    // 17-11: patch level / subminor (7 bits)
    // 10-4: year since 2000 (7 bits)
    // 3-0: month (4 bits)
    uint32_t major = (osVal >> 25) & 0x7F;
    uint32_t minor = (osVal >> 18) & 0x7F;
    uint32_t patch = (osVal >> 11) & 0x7F;
    
    uint32_t year = 2000 + ((osVal >> 4) & 0x7F);
    uint32_t month = osVal & 0x0F;

    if (major != 0 || minor != 0 || patch != 0) {
        osVersion = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
    
    if (month >= 1 && month <= 12 && year >= 2000 && year <= 2100) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%04u-%02u", year, month);
        osPatchLevel = buf;
    }
}

std::string searchKernelRelease(const std::filesystem::path& path, uint64_t offset, uint64_t size) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "Unknown";

    in.seekg(offset);
    // Limit kernel search scan to first 4MB to avoid memory bloat
    uint64_t scanSize = std::min(size, static_cast<uint64_t>(4 * 1024 * 1024));
    std::vector<char> buffer(scanSize);
    in.read(buffer.data(), scanSize);
    
    std::string_view view(buffer.data(), in.gcount());
    size_t pos = view.find("Linux version ");
    if (pos != std::string_view::npos) {
        std::string line;
        for (size_t i = pos; i < view.length() && view[i] != '\0' && view[i] != '\n' && view[i] != '\r'; ++i) {
            line.push_back(view[i]);
            if (line.length() > 128) break; // Limit string length
        }
        return line;
    }
    return "Unknown (Compressed)";
}

} // namespace

BootImgInfo BootImgParser::parse(const std::filesystem::path& path) {
    BootImgInfo info;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        LOG_ERROR("BootImgParser: Failed to open file: {}", path.string());
        return info;
    }

    // Read first 2048 bytes representing the header block
    std::vector<char> header(2048, 0);
    in.read(header.data(), header.size());
    if (in.gcount() < 64) {
        LOG_WARN("BootImgParser: File too small to contain valid boot image header");
        return info;
    }

    // Check magic: "ANDROID!" (8 bytes)
    if (std::memcmp(header.data(), "ANDROID!", 8) != 0) {
        LOG_WARN("BootImgParser: Invalid magic signature");
        return info;
    }

    info.isValid = true;

    // Header version field is located at offset 40 (bytes 40-43) in all header layouts
    uint32_t headerVersion = 0;
    std::memcpy(&headerVersion, header.data() + 40, sizeof(headerVersion));
    info.headerVersion = headerVersion;

    uint32_t osVal = 0;

    if (headerVersion < 3) {
        // v0, v1, v2 headers
        uint32_t kernelSize = 0;
        uint32_t ramdiskSize = 0;
        uint32_t pageSize = 0;

        std::memcpy(&kernelSize, header.data() + 8, sizeof(kernelSize));
        std::memcpy(&ramdiskSize, header.data() + 16, sizeof(ramdiskSize));
        std::memcpy(&pageSize, header.data() + 36, sizeof(pageSize));
        std::memcpy(&osVal, header.data() + 44, sizeof(osVal));

        info.kernelSize = kernelSize;
        info.ramdiskSize = ramdiskSize;
        info.pageSize = pageSize;

        // Command line starts at offset 64 (512 bytes)
        info.cmdline = std::string(header.data() + 64, strnlen(header.data() + 64, 512));

        LOG_INFO("Parsed Legacy Boot Header (v{}): page_size={}, kernel_size={}, ramdisk_size={}", 
            headerVersion, pageSize, kernelSize, ramdiskSize);
    } else {
        // v3, v4 headers
        uint32_t kernelSize = 0;
        uint32_t ramdiskSize = 0;

        std::memcpy(&kernelSize, header.data() + 8, sizeof(kernelSize));
        std::memcpy(&ramdiskSize, header.data() + 12, sizeof(ramdiskSize));
        std::memcpy(&osVal, header.data() + 16, sizeof(osVal));

        info.kernelSize = kernelSize;
        info.ramdiskSize = ramdiskSize;
        info.pageSize = 4096; // Header v3/v4 assumes page size of 4096 bytes

        // Command line is at offset 44 (1536 bytes)
        info.cmdline = std::string(header.data() + 44, strnlen(header.data() + 44, 1536));

        LOG_INFO("Parsed Modern Boot Header (v{}): kernel_size={}, ramdisk_size={}", 
            headerVersion, kernelSize, ramdiskSize);
    }

    decodeOsVersion(osVal, info.osVersion, info.osPatchLevel);

    // If kernel size is valid, scan for uncompressed kernel version string
    if (info.kernelSize > 0 && info.pageSize > 0) {
        uint64_t kernelOffset = info.pageSize; // Kernel starts right after the header page
        info.kernelRelease = searchKernelRelease(path, kernelOffset, info.kernelSize);
    }

    return info;
}
