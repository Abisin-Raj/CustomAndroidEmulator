#include "boot_assets/boot_asset_loader.h"
#include "core/logger.h"
#include <fstream>

std::vector<char> BootAssetLoader::loadBuffer(const std::filesystem::path& bootImg, uint64_t offset, uint64_t size) {
    std::vector<char> buffer;
    if (size == 0) {
        LOG_PARSER_WARN("BootAssetLoader: Request to load 0 bytes from boot image.");
        return buffer;
    }

    std::ifstream in(bootImg, std::ios::binary);
    if (!in) {
        LOG_PARSER_ERROR("BootAssetLoader: Failed to open boot image for asset extraction: {}", bootImg.string());
        return buffer;
    }

    in.seekg(0, std::ios::end);
    uint64_t fileSize = in.tellg();
    if (offset + size > fileSize) {
        LOG_PARSER_ERROR("BootAssetLoader: Requested segment range [{}, {}] exceeds boot.img file size ({} bytes)", 
            offset, offset + size, fileSize);
        return buffer;
    }

    in.seekg(offset);
    buffer.resize(size);
    in.read(buffer.data(), size);
    
    if (static_cast<uint64_t>(in.gcount()) != size) {
        LOG_PARSER_ERROR("BootAssetLoader: Failed to read complete segment block of size {} from offset {}", size, offset);
        buffer.clear();
    }

    return buffer;
}

bool BootAssetLoader::writeTempFile(const std::vector<char>& buffer, const std::filesystem::path& destFile) {
    if (buffer.empty()) return false;

    // Create directories if necessary
    std::filesystem::create_directories(destFile.parent_path());

    std::ofstream out(destFile, std::ios::binary);
    if (!out) {
        LOG_PARSER_ERROR("BootAssetLoader: Failed to create temporary boot asset file: {}", destFile.string());
        return false;
    }

    out.write(buffer.data(), buffer.size());
    return true;
}

void BootAssetLoader::cleanTempAssets(const std::filesystem::path& tempDir) {
    if (std::filesystem::exists(tempDir)) {
        try {
            std::filesystem::remove_all(tempDir);
            LOG_BOOT_INFO("BootAssetLoader: Cleaned up temporary boot assets directory: {}", tempDir.string());
        } catch (const std::exception& e) {
            LOG_BOOT_ERROR("BootAssetLoader: Failed to clean up temporary assets: {}", e.what());
        }
    }
}
