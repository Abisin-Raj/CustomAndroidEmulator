#pragma once
#include <vector>
#include <string>
#include <filesystem>

class BootAssetLoader {
public:
    /**
     * @brief Load a specific segment of the boot image into memory.
     */
    static std::vector<char> loadBuffer(const std::filesystem::path& bootImg, uint64_t offset, uint64_t size);

    /**
     * @brief Writes a memory buffer to a temporary file.
     */
    static bool writeTempFile(const std::vector<char>& buffer, const std::filesystem::path& destFile);

    /**
     * @brief Cleans up/removes temporary files or directories.
     */
    static void cleanTempAssets(const std::filesystem::path& tempDir);
};
