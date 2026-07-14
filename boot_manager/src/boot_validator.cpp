#include "boot_manager/boot_manager.h"
#include "image_parser/bootimg_parser.h"
#include "image_parser/vbmeta_parser.h"
#include "image_parser/payload_parser.h"
#include "image_parser/super_parser.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

BootValidationReport BootManager::validateAndPrepareBoot(const std::string& romName, const std::filesystem::path& romDir) {
    LOG_BOOT_INFO("Starting boot readiness validation for ROM '{}' in '{}'", romName, romDir.string());
    
    BootValidationReport report;
    report.vmProfile = "generic_arm64"; // Default profile

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
    if (!hasSystemImg && !hasSuperImg && !hasPayloadBin) {
        report.errors.push_back("Missing filesystem partition. ROM must contain system.img, super.img, or payload.bin.");
    }

    if (hasSuperImg) {
        report.warnings.push_back("Dynamic partitions container (super.img) detected.");
        report.recommendations.push_back("Use logical partition mounts in target layout.");
    }

    if (hasPayloadBin) {
        report.warnings.push_back("Unextracted OTA package (payload.bin) detected. Extracted partitions will run dynamically.");
        report.recommendations.push_back("A/B layout detected. Ensure slot selection matches active boot target.");
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

    // 5. Write boot_config.json (VM profile description) if bootable checks succeeded
    if (report.status != "Not Bootable") {
        nlohmann::json config;
        config["version"] = 1;
        config["boot_image"] = "boot.img";
        config["kernel_offset"] = kernelOffset;
        config["kernel_size"] = bootInfo.kernelSize;
        config["ramdisk_offset"] = ramdiskOffset;
        config["ramdisk_size"] = bootInfo.ramdiskSize;
        config["architecture"] = bootInfo.headerVersion >= 3 ? "arm64" : "arm64"; // Default ARM64
        config["dynamic_partitions"] = (hasSuperImg || hasPayloadBin);
        config["verified_boot"] = avbEnabled;
        config["vm_profile"] = report.vmProfile;

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
