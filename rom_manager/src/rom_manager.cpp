#include "rom_manager/rom_manager.h"
#include "core/logger.h"
#include "image_parser/image_parser.h"
#include "boot_manager/boot_manager.h"
#include <archive.h>
#include <archive_entry.h>
#include <nlohmann/json.hpp>
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
        LOG_ROM_ERROR("Failed to create ROMs directory '{}': {}", m_romsDir.string(), e.what());
    }
}

bool RomManager::validateRomZip(const std::string& zipPath, RomMetadata& outMetadata, std::string& outError) {
    LOG_ROM_INFO("Validating zip package: {}", zipPath);
    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);
    archive_read_support_filter_all(a);

    int r = archive_read_open_filename(a, zipPath.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        outError = "Failed to open ZIP file: " + std::string(archive_error_string(a));
        LOG_ROM_ERROR("validateRomZip failed. {}", outError);
        archive_read_free(a);
        return false;
    }

    struct archive_entry* entry;
    bool hasImages = false;
    bool hasMetadataFile = false;
    std::string metadataContent;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string name = archive_entry_pathname(entry);
        
        if (name.ends_with(".img") || name.ends_with(".bin") || name.ends_with(".elf") || name == "payload.bin") {
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
                LOG_ROM_WARN("Failed to read metadata file from ZIP.");
            }
        }
    }

    archive_read_free(a);

    if (!hasImages) {
        outError = "ZIP file does not contain valid Android image files (.img, .bin, payload.bin, etc.)";
        LOG_ROM_WARN("validateRomZip failed. {}", outError);
        return false;
    }

    outMetadata.metadataVersion = 1;
    outMetadata.name = std::filesystem::path(zipPath).stem().string();
    outMetadata.androidVersion = "Unknown";
    outMetadata.device = "Generic Device";
    outMetadata.codename = "unknown";
    outMetadata.architecture = "arm64";
    outMetadata.status = "Valid";

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
                    outMetadata.codename = val;
                } else if (key == "post-build") {
                    outMetadata.androidVersion = parseAndroidVersion(val);
                }
            }
        }
    }

    LOG_ROM_INFO("Validation successful. Detected ROM: name='{}', device='{}', android='{}'", 
        outMetadata.name, outMetadata.device, outMetadata.androidVersion);

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
        LOG_ROM_WARN("Install failed. ROM folder already exists: {}", targetFolder.string());
        return false;
    }

    LOG_ROM_INFO("Installing ROM '{}' to '{}'", meta.name, targetFolder.string());
    if (!std::filesystem::create_directories(targetFolder)) {
        outError = "Failed to create target directory: " + targetFolder.string();
        LOG_ROM_ERROR("Install failed. {}", outError);
        return false;
    }

    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);
    archive_read_support_filter_all(a);

    int r = archive_read_open_filename(a, zipPath.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        outError = "Failed to open ZIP for extraction: " + std::string(archive_error_string(a));
        LOG_ROM_ERROR("Install failed. {}", outError);
        archive_read_free(a);
        return false;
    }

    struct archive_entry* entry;
    bool extractionSuccess = true;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string name = archive_entry_pathname(entry);

        bool isExtractable = (name.ends_with(".img") || name.ends_with(".bin") || name.ends_with(".elf") || 
                              name == "payload.bin" || name == "META-INF/com/android/metadata");

        if (isExtractable) {
            std::filesystem::path destFile = targetFolder / name;
            LOG_ROM_INFO("Extracting '{}' to '{}'", name, destFile.string());
            
            std::filesystem::create_directories(destFile.parent_path());

            std::ofstream out(destFile, std::ios::binary);
            if (!out) {
                outError = "Failed to create destination file: " + destFile.string();
                LOG_ROM_ERROR("Install failed during extraction. {}", outError);
                extractionSuccess = false;
                break;
            }

            char buffer[32768];
            la_ssize_t size;
            while ((size = archive_read_data(a, buffer, sizeof(buffer))) > 0) {
                out.write(buffer, size);
            }

            if (size < 0) {
                outError = "Error reading archive entry: " + std::string(archive_error_string(a));
                LOG_ROM_ERROR("Install failed during extraction data stream. {}", outError);
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

    // Run Image Inspection Engine on target folder to populate detailed information
    LOG_ROM_INFO("Extract complete. Running ImageParser inspection...");
    auto inspection = ImageParserEngine::inspectRomDirectory(targetFolder);

    // Run Boot readiness validation and configuration generation
    LOG_ROM_INFO("Running BootManager verification...");
    auto bootReport = BootManager::validateAndPrepareBoot(meta.name, targetFolder);

    // Populate advanced metadata
    meta.kernel = inspection.kernelRelease;
    meta.partitionLayout = inspection.partitionLayout;
    meta.images = inspection.images;
    meta.avbStatus = inspection.avbEnabled ? "Active (VBMeta verified)" : "Disabled / Strip-off";
    
    if (inspection.osVersion != "Unknown") {
        meta.androidVersion = inspection.osVersion;
    }

    meta.status = bootReport.status;

    if (!saveMetadata(targetFolder, meta)) {
        outError = "Failed to save metadata.json in ROM directory.";
        LOG_ROM_ERROR("Install failed. {}", outError);
        std::filesystem::remove_all(targetFolder);
        return false;
    }

    LOG_ROM_INFO("Installation of '{}' completed successfully. Status: {}", meta.name, meta.status);
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
        LOG_ROM_INFO("ROM '{}' uninstalled successfully from '{}'", romName, targetFolder.string());
        return true;
    } catch (const std::exception& e) {
        outError = std::string("Failed to remove ROM directory: ") + e.what();
        LOG_ROM_ERROR("Uninstall failed. {}", outError);
        return false;
    }
}

bool RomManager::saveMetadata(const std::filesystem::path& folder, const RomMetadata& meta) const {
    nlohmann::json j;
    j["metadata_version"] = meta.metadataVersion;
    j["name"] = meta.name;
    j["device"] = meta.device;
    j["codename"] = meta.codename;
    j["android_version"] = meta.androidVersion;
    j["architecture"] = meta.architecture;
    j["kernel"] = meta.kernel;
    j["partition_layout"] = meta.partitionLayout;
    j["images"] = meta.images;
    j["avb_status"] = meta.avbStatus;
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
        
        outMeta.metadataVersion = j.value("metadata_version", 1);
        outMeta.name = j.value("name", "");
        outMeta.device = j.value("device", "Generic Device");
        outMeta.codename = j.value("codename", "unknown");
        outMeta.androidVersion = j.value("android_version", "Unknown");
        outMeta.architecture = j.value("architecture", "arm64");
        outMeta.kernel = j.value("kernel", "Unknown");
        outMeta.partitionLayout = j.value("partition_layout", "A-only");
        outMeta.avbStatus = j.value("avb_status", "Unknown");
        outMeta.status = j.value("status", "");

        if (j.contains("images")) {
            outMeta.images = j["images"].get<std::vector<std::string>>();
        }

        return true;
    } catch (const std::exception& e) {
        LOG_ROM_ERROR("Failed to parse metadata.json in '{}': {}", folder.string(), e.what());
        return false;
    }
}
