#pragma once

#include "models/rom_metadata.h"
#include <filesystem>

#include <functional>

class RomManager {
public:
    explicit RomManager(const std::string& romsDir);

    /**
     * @brief Validates if the ZIP file contains valid Android partition images (or payload.bin)
     *        and parses metadata from it if possible.
     */
    bool validateRomZip(const std::string& zipPath, RomMetadata& outMetadata, std::string& outError);

    /**
     * @brief Installs a custom ROM from a ZIP file by extracting its images and writing metadata.json.
     */
    bool installRom(const std::string& zipPath, const std::string& romName, std::string& outError, std::function<void(const std::string&, float)> progressCb = nullptr);

    /**
     * @brief Scans the roms directory and returns metadata of all installed ROMs.
     */
    std::vector<RomMetadata> getInstalledRoms() const;

    /**
     * @brief Uninstalls a ROM by removing its directory.
     */
    bool uninstallRom(const std::string& romName, std::string& outError);

private:
    std::filesystem::path m_romsDir;

    void ensureRomsDirExists() const;
    bool saveMetadata(const std::filesystem::path& folder, const RomMetadata& meta) const;
    bool loadMetadata(const std::filesystem::path& folder, RomMetadata& outMeta) const;
};
