#include "boot_manager/boot_manager.h"
#include "image_parser/bootimg_parser.h"
#include "image_parser/vbmeta_parser.h"
#include "image_parser/payload_parser.h"
#include "image_parser/super_parser.h"
#include "image_converter/brotli_decompressor.h"
#include "image_converter/dat_image_builder.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace {
std::string detectCodename(const std::filesystem::path& romDir) {
    std::filesystem::path metaPath = romDir / "META-INF" / "com" / "android" / "metadata";
    if (std::filesystem::exists(metaPath)) {
        std::ifstream in(metaPath);
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("pre-device=", 0) == 0) {
                // Trim trailing newline or carriage return
                std::string dev = line.substr(11);
                if (!dev.empty() && dev.back() == '\r') dev.pop_back();
                return dev;
            }
        }
    }
    return "unknown";
}
} // namespace

BootValidationReport BootManager::validateAndPrepareBoot(const std::string& romName, const std::filesystem::path& romDir) {
    LOG_BOOT_INFO("Starting boot readiness validation for ROM '{}' in '{}'", romName, romDir.string());
    
    BootValidationReport report;
    std::string codename = detectCodename(romDir);
    
    // Compatibility Layer: Route board codenames to specific hardware profiles
    if (codename == "violet") {
        report.vmProfile = "violet";
        report.recommendations.push_back("Xiaomi Redmi Note 7 Pro hardware profile selected.");
    } else {
        report.vmProfile = "generic_arm64"; // Fallback default profile
    }

    std::filesystem::path bootImgPath = romDir / "boot.img";
    std::filesystem::path vbmetaImgPath = romDir / "vbmeta.img";
    std::filesystem::path superImgPath = romDir / "super.img";
    std::filesystem::path systemImgPath = romDir / "system.img";
    std::filesystem::path payloadBinPath = romDir / "payload.bin";
    std::filesystem::path vendorBootImgPath = romDir / "vendor_boot.img";

    bool hasBootImg = std::filesystem::exists(bootImgPath);
    bool hasVbmetaImg = std::filesystem::exists(vbmetaImgPath);
    bool hasSuperImg = std::filesystem::exists(superImgPath);
    bool hasSystemImg = std::filesystem::exists(systemImgPath);
    bool hasPayloadBin = std::filesystem::exists(payloadBinPath);
    bool hasVendorBootImg = std::filesystem::exists(vendorBootImgPath);
    bool hasSystemNewDatBr = std::filesystem::exists(romDir / "system.new.dat.br");
    bool hasSystemNewDat = std::filesystem::exists(romDir / "system.new.dat");
    bool hasVendorNewDatBr = std::filesystem::exists(romDir / "vendor.new.dat.br");
    bool hasSystemTransferList = std::filesystem::exists(romDir / "system.transfer.list");
    bool hasVendorTransferList = std::filesystem::exists(romDir / "vendor.transfer.list");

    BootImgInfo bootInfo;
    uint64_t kernelOffset = 0;
    uint64_t ramdiskOffset = 0;

    // 1. Validate boot.img
    if (!hasBootImg) {
        report.errors.push_back("Missing boot.img file. A valid kernel is required to boot.");
    } else {
        bootInfo = BootImgParser::parse(bootImgPath);
        if (!bootInfo.isValid) {
            report.errors.push_back("boot.img has a corrupt header or is not a valid Android boot partition.");
        } else {
            kernelOffset = bootInfo.getKernelOffset();
            ramdiskOffset = bootInfo.getRamdiskOffset();

            if (bootInfo.kernelSize == 0) {
                report.errors.push_back("boot.img contains an empty kernel block (0 bytes).");
            }
            if (bootInfo.ramdiskSize == 0) {
                report.warnings.push_back("boot.img contains an empty ramdisk block. Ensure vendor_boot contains the ramdisk.");
            }

            // Check modern headers requiring vendor_boot
            if (bootInfo.headerVersion >= 3) {
                report.warnings.push_back("Modern boot header version " + std::to_string(bootInfo.headerVersion) + " detected.");
                if (!hasVendorBootImg) {
                    report.warnings.push_back("vendor_boot.img is missing. Android GKI v3/v4 usually requires a separate vendor ramdisk.");
                } else {
                    report.recommendations.push_back("vendor_boot.img detected. Will parse vendor ramdisk offsets in Phase 4.");
                }
            }
        }
    }

    // 2. Validate Filesystem partitions
    if (!hasSystemImg && !hasSuperImg && !hasPayloadBin && !hasSystemNewDatBr && !hasSystemNewDat) {
        report.errors.push_back("Missing filesystem partition. ROM must contain system.img, super.img, payload.bin, or system.new.dat.br.");
    }

    if (hasSuperImg) {
        report.warnings.push_back("Dynamic partitions container (super.img) detected.");
        report.recommendations.push_back("Use logical partition mounts in target layout.");
    }

    if (hasPayloadBin) {
        report.warnings.push_back("Unextracted OTA package (payload.bin) detected. Extracted partitions will run dynamically.");
        report.recommendations.push_back("A/B layout detected. Ensure slot selection matches active boot target.");
    }

    if (hasSystemNewDatBr && hasSystemTransferList) {
        report.recommendations.push_back("Brotli OTA format detected. Will reconstruct system.img and vendor.img.");
    } else if (hasSystemNewDatBr) {
        report.warnings.push_back("system.new.dat.br found but system.transfer.list is missing.");
    }

    if (hasSystemNewDat) {
        report.warnings.push_back("Sparse block system partition (system.new.dat) detected.");
        report.recommendations.push_back("System partition will be converted to raw ext4 dynamically in a later phase.");
    }

    // 3. Validate AVB (vbmeta.img)
    bool avbEnabled = false;
    if (hasVbmetaImg) {
        auto vbmetaInfo = VbmetaParser::parse(vbmetaImgPath);
        if (vbmetaInfo.isValid) {
            avbEnabled = vbmetaInfo.avbEnabled;
            if (avbEnabled) {
                report.warnings.push_back("Android Verified Boot (AVB 2.0) is enabled.");
                report.recommendations.push_back("AVB is active. Enable vbmeta verify bypass flags in VM arguments.");
            } else {
                report.recommendations.push_back("Verified Boot signature checks are disabled via flag mask.");
            }
        } else {
            report.errors.push_back("vbmeta.img is corrupt or has invalid AVB signature.");
        }
    } else {
        report.warnings.push_back("Missing vbmeta.img. Android Verified Boot checks will be bypassed.");
    }

    // 4. Default recommendations
    report.recommendations.push_back("Use virtual ARM64 profile.");

    // Determine overall status
    if (!report.errors.empty()) {
        report.status = "Not Bootable";
        LOG_BOOT_ERROR("ROM '{}' is validation failed (Not Bootable). {} errors, {} warnings.", 
            romName, report.errors.size(), report.warnings.size());
    } else if (!report.warnings.empty()) {
        report.status = "Warning";
        LOG_BOOT_WARN("ROM '{}' is ready with warnings. {} warnings.", romName, report.warnings.size());
    } else {
        report.status = "Ready";
        LOG_BOOT_INFO("ROM '{}' successfully passed all boot checks. Ready for virtualization.", romName);
    }

    // 5. Reconstruct system/vendor images from Brotli OTA format
    std::string systemImagePath;
    std::string systemImageSha256;
    bool        systemImageVerified = false;
    std::string vendorImagePath;
    std::string vendorImageSha256;
    bool        vendorImageVerified = false;

    if (report.status != "Not Bootable" && hasSystemNewDatBr && hasSystemTransferList) {
        auto sysNewDat = romDir / "system.new.dat";
        auto sysImg    = romDir / "system.img";
        auto sysTransfer = romDir / "system.transfer.list";

        if (!std::filesystem::exists(sysImg)) {
            LOG_BOOT_INFO("Decompressing system.new.dat.br...");
            std::string err;
            bool ok = BrotliDecompressor::decompress(romDir / "system.new.dat.br", sysNewDat, nullptr, &err);
            if (!ok) {
                LOG_BOOT_ERROR("Brotli decompression failed: {}", err);
                report.warnings.push_back("system.new.dat.br decompression failed: " + err);
            } else {
                LOG_BOOT_INFO("Reconstructing system.img from transfer list...");
                ok = DatImageBuilder::build(sysNewDat, sysTransfer, sysImg, nullptr, &err);
                if (!ok) {
                    LOG_BOOT_ERROR("system.img reconstruction failed: {}", err);
                    report.warnings.push_back("system.img reconstruction failed: " + err);
                } else {
                    // Remove intermediate .new.dat to save disk space
                    std::filesystem::remove(sysNewDat);
                    auto vr = DatImageBuilder::verify(sysImg);
                    systemImageVerified = vr.isValidExt4;
                    systemImageSha256   = vr.sha256;
                    systemImagePath     = "system.img";
                    LOG_BOOT_INFO("system.img reconstructed: valid_ext4={}, sha256={}",
                                  vr.isValidExt4 ? "true" : "false", vr.sha256.substr(0, 16) + "...");
                    if (!vr.isValidExt4) {
                        report.warnings.push_back("system.img reconstruction completed but ext4 magic check failed.");
                    }
                }
            }
        } else {
            // Image already reconstructed from a previous install
            auto vr = DatImageBuilder::verify(sysImg);
            systemImageVerified = vr.isValidExt4;
            systemImageSha256   = vr.sha256;
            systemImagePath     = "system.img";
            LOG_BOOT_INFO("system.img already exists: valid_ext4={}", vr.isValidExt4 ? "true" : "false");
        }

        // Repeat for vendor
        if (hasVendorNewDatBr && hasVendorTransferList) {
            auto vendNewDat  = romDir / "vendor.new.dat";
            auto vendImg     = romDir / "vendor.img";
            auto vendTransfer = romDir / "vendor.transfer.list";

            if (!std::filesystem::exists(vendImg)) {
                LOG_BOOT_INFO("Decompressing vendor.new.dat.br...");
                std::string err;
                bool ok = BrotliDecompressor::decompress(romDir / "vendor.new.dat.br", vendNewDat, nullptr, &err);
                if (ok) {
                    ok = DatImageBuilder::build(vendNewDat, vendTransfer, vendImg, nullptr, &err);
                    if (ok) {
                        std::filesystem::remove(vendNewDat);
                        auto vr = DatImageBuilder::verify(vendImg);
                        vendorImageVerified = vr.isValidExt4;
                        vendorImageSha256   = vr.sha256;
                        vendorImagePath     = "vendor.img";
                        LOG_BOOT_INFO("vendor.img reconstructed: valid_ext4={}, sha256={}",
                                      vr.isValidExt4 ? "true" : "false", vr.sha256.substr(0, 16) + "...");
                    } else {
                        LOG_BOOT_WARN("vendor.img reconstruction failed: {}", err);
                    }
                } else {
                    LOG_BOOT_WARN("vendor.new.dat.br decompression failed: {}", err);
                }
            } else {
                auto vr = DatImageBuilder::verify(vendImg);
                vendorImageVerified = vr.isValidExt4;
                vendorImageSha256   = vr.sha256;
                vendorImagePath     = "vendor.img";
            }
        }
    }

    // 6. Write boot_config.json
    if (report.status != "Not Bootable") {
        nlohmann::json config;
        config["version"] = 1;
        config["boot_image"] = "boot.img";
        config["kernel_offset"] = kernelOffset;
        config["kernel_size"] = bootInfo.kernelSize;
        config["ramdisk_offset"] = ramdiskOffset;
        config["ramdisk_size"] = bootInfo.ramdiskSize;
        config["architecture"] = "arm64";
        config["dynamic_partitions"] = (hasSuperImg || hasPayloadBin);
        config["verified_boot"] = avbEnabled;
        config["vm_profile"] = report.vmProfile;
        config["cmdline"] = bootInfo.cmdline;

        if (!systemImagePath.empty()) {
            config["system_image"]          = systemImagePath;
            config["system_image_sha256"]   = systemImageSha256;
            config["system_image_verified"] = systemImageVerified;
        }
        if (!vendorImagePath.empty()) {
            config["vendor_image"]          = vendorImagePath;
            config["vendor_image_sha256"]   = vendorImageSha256;
            config["vendor_image_verified"] = vendorImageVerified;
        }

        std::filesystem::path configPath = romDir / "boot_config.json";
        std::ofstream out(configPath);
        if (out) {
            out << config.dump(4);
            LOG_BOOT_INFO("Wrote VM boot configuration to: {}", configPath.string());
        } else {
            LOG_BOOT_ERROR("Failed to write boot_config.json in ROM folder.");
        }
    }

    return report;
}
