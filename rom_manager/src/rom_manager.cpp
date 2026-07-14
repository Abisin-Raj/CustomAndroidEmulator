#include "rom_manager/rom_manager.h"
#include <archive.h>
#include <archive_entry.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace {

std::string trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();
    return (start < end) ? std::string(start, end) : "";
}

std::string parseAndroidVersion(const std::string& postBuild) {
    auto colonPos = postBuild.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < postBuild.length()) {
        std::string right = postBuild.substr(colonPos + 1);
        auto slashPos = right.find('/');
        if (slashPos != std::string::npos) {
            return right.substr(0, slashPos);
        }
    }
    return "Unknown";
}

} // namespace

RomManager::RomManager(const std::string& romsDir)
    : m_romsDir(std::filesystem::absolute(romsDir)) {
    ensureRomsDirExists();
}

void RomManager::ensureRomsDirExists() const {
    try {
        if (!std::filesystem::exists(m_romsDir)) {
            std::filesystem::create_directories(m_romsDir);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to create ROMs directory '{}': {}", m_romsDir.string(), e.what());
    }
}

bool RomManager::validateRomZip(const std::string& zipPath, RomMetadata& outMetadata, std::string& outError) {
    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);
    archive_read_support_filter_all(a);

    int r = archive_read_open_filename(a, zipPath.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        outError = "Failed to open ZIP file: " + std::string(archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    struct archive_entry* entry;
    bool hasImages = false;
    bool hasMetadataFile = false;
    std::string metadataContent;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string name = archive_entry_pathname(entry);
        
        // Accept common images or payload.bin
        if (name == "boot.img" || name == "system.img" || name == "vendor.img" || 
            name == "product.img" || name == "payload.bin" || name.ends_with(".img")) {
            hasImages = true;
        }

        if (name == "META-INF/com/android/metadata") {
            hasMetadataFile = true;
            size_t size = archive_entry_size(entry);
            metadataContent.resize(size);
            la_ssize_t bytesRead = archive_read_data(a, &metadataContent[0], size);
            if (bytesRead >= 0) {
                metadataContent.resize(bytesRead);
            } else {
                spdlog::warn("Failed to read metadata file content from archive");
            }
        }
    }

    archive_read_free(a);

    if (!hasImages) {
        outError = "ZIP file does not contain valid Android image files (boot.img, system.img, payload.bin, etc.)";
        return false;
    }

    // Default metadata
    outMetadata.name = std::filesystem::path(zipPath).stem().string();
    outMetadata.androidVersion = "Unknown";
    outMetadata.device = "Generic Device";
    outMetadata.architecture = "arm64";
    outMetadata.status = "Valid";

    // Parse metadata properties
    if (hasMetadataFile && !metadataContent.empty()) {
        std::stringstream ss(metadataContent);
        std::string line;
        while (std::getline(ss, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            auto eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string key = trim(line.substr(0, eqPos));
                std::string val = trim(line.substr(eqPos + 1));
                if (key == "pre-device") {
                    outMetadata.device = val;
                } else if (key == "post-build") {
                    outMetadata.androidVersion = parseAndroidVersion(val);
                }
            }
        }
    }

    return true;
}

bool RomManager::installRom(const std::string& zipPath, const std::string& romName, std::string& outError) {
    RomMetadata meta;
    if (!validateRomZip(zipPath, meta, outError)) {
        return false;
    }

    if (!romName.empty()) {
        meta.name = romName;
    }

    std::filesystem::path targetFolder = m_romsDir / meta.name;
    if (std::filesystem::exists(targetFolder)) {
        outError = "A ROM with the name '" + meta.name + "' is already installed.";
        return false;
    }

    spdlog::info("Installing ROM '{}' to '{}'", meta.name, targetFolder.string());
    if (!std::filesystem::create_directories(targetFolder)) {
        outError = "Failed to create target directory: " + targetFolder.string();
        return false;
    }

    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);
    archive_read_support_filter_all(a);

    int r = archive_read_open_filename(a, zipPath.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        outError = "Failed to open ZIP for extraction: " + std::string(archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    struct archive_entry* entry;
    bool extractionSuccess = true;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string name = archive_entry_pathname(entry);

        if (name == "boot.img" || name == "system.img" || name == "vendor.img" || 
            name == "product.img" || name == "payload.bin" || name.ends_with(".img")) {
            
            std::filesystem::path destFile = targetFolder / name;
            spdlog::info("Extracting '{}'...", name);

            std::ofstream out(destFile, std::ios::binary);
            if (!out) {
                outError = "Failed to create destination file: " + destFile.string();
                extractionSuccess = false;
                break;
            }

            char buffer[16384];
            la_ssize_t size;
            while ((size = archive_read_data(a, buffer, sizeof(buffer))) > 0) {
                out.write(buffer, size);
            }

            if (size < 0) {
                outError = "Error reading archive entry: " + std::string(archive_error_string(a));
                extractionSuccess = false;
                break;
            }
        }
    }

    archive_read_free(a);

    if (!extractionSuccess) {
        std::filesystem::remove_all(targetFolder);
        return false;
    }

    meta.status = "Installed";
    if (!saveMetadata(targetFolder, meta)) {
        outError = "Failed to save metadata.json in ROM directory.";
        std::filesystem::remove_all(targetFolder);
        return false;
    }

    return true;
}

std::vector<RomMetadata> RomManager::getInstalledRoms() const {
    std::vector<RomMetadata> roms;
    if (!std::filesystem::exists(m_romsDir)) return roms;

    for (const auto& entry : std::filesystem::directory_iterator(m_romsDir)) {
        if (entry.is_directory()) {
            RomMetadata meta;
            if (loadMetadata(entry.path(), meta)) {
                roms.push_back(meta);
            }
        }
    }
    return roms;
}

bool RomManager::uninstallRom(const std::string& romName, std::string& outError) {
    std::filesystem::path targetFolder = m_romsDir / romName;
    if (!std::filesystem::exists(targetFolder)) {
        outError = "ROM '" + romName + "' is not installed.";
        return false;
    }

    try {
        std::filesystem::remove_all(targetFolder);
        spdlog::info("ROM '{}' uninstalled successfully", romName);
        return true;
    } catch (const std::exception& e) {
        outError = std::string("Failed to remove ROM directory: ") + e.what();
        return false;
    }
}

bool RomManager::saveMetadata(const std::filesystem::path& folder, const RomMetadata& meta) const {
    nlohmann::json j;
    j["name"] = meta.name;
    j["android_version"] = meta.androidVersion;
    j["device"] = meta.device;
    j["architecture"] = meta.architecture;
    j["status"] = meta.status;

    std::filesystem::path path = folder / "metadata.json";
    std::ofstream out(path);
    if (!out) return false;

    out << j.dump(4);
    return true;
}

bool RomManager::loadMetadata(const std::filesystem::path& folder, RomMetadata& outMeta) const {
    std::filesystem::path path = folder / "metadata.json";
    std::ifstream in(path);
    if (!in) return false;

    try {
        nlohmann::json j;
        in >> j;
        outMeta.name = j.value("name", "");
        outMeta.androidVersion = j.value("android_version", "");
        outMeta.device = j.value("device", "");
        outMeta.architecture = j.value("architecture", "");
        outMeta.status = j.value("status", "");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse metadata.json in '{}': {}", folder.string(), e.what());
        return false;
    }
}
