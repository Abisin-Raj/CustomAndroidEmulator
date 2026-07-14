#pragma once
#include <filesystem>
#include <functional>
#include <string>

/// Result of an image integrity check performed after reconstruction.
struct ImageVerificationResult {
    bool        isValidExt4 = false;  ///< true if ext4 superblock magic (0x53EF) found at offset 1080.
    std::string sha256;               ///< Hex SHA-256 digest of the entire image file.
    uint64_t    sizeBytes   = 0;      ///< File size in bytes.
};

/// Reconstructs a raw ext4 image from an Android OTA .new.dat + transfer.list pair,
/// then verifies the result.
class DatImageBuilder {
public:
    static constexpr uint32_t kBlockSize = 4096; ///< Android OTA block size (always 4 096 bytes).

    /// Reconstruct @p datFile using @p transferListFile, writing output to @p outputImage.
    /// @param progressCb  Called with [0.0, 1.0] fraction as blocks are written.
    /// @return true on success.
    static bool build(const std::filesystem::path& datFile,
                      const std::filesystem::path& transferListFile,
                      const std::filesystem::path& outputImage,
                      std::function<void(float)>   progressCb = nullptr,
                      std::string*                 errorOut   = nullptr);

    /// Verify an ext4 image: check superblock magic and compute SHA-256.
    static ImageVerificationResult verify(const std::filesystem::path& imagePath);
};
