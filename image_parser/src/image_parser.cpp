#include "image_parser/image_parser.h"
#include "image_parser/bootimg_parser.h"
#include "image_parser/vbmeta_parser.h"
#include "image_parser/payload_parser.h"
#include "image_parser/super_parser.h"
#include "image_parser/ext4_parser.h"
#include "core/logger.h"

RomInspection ImageParserEngine::inspectRomDirectory(const std::filesystem::path& romDir) {
    RomInspection inspection;
    if (!std::filesystem::exists(romDir)) {
        LOG_PARSER_ERROR("Target directory does not exist: {}", romDir.string());
        return inspection;
    }

    LOG_PARSER_INFO("Inspecting directory: {}", romDir.string());

    bool hasPayload = false;
    bool hasSuper = false;

    for (const auto& entry : std::filesystem::directory_iterator(romDir)) {
        if (entry.is_regular_file()) {
            std::filesystem::path filePath = entry.path();
            std::string filename = filePath.filename().string();
            std::string ext = filePath.extension().string();

            if (ext == ".img" || ext == ".bin" || ext == ".elf") {
                inspection.images.push_back(filename);
            }

            if (filename == "boot.img") {
                LOG_PARSER_INFO("Found boot.img, parsing...");
                auto bootInfo = BootImgParser::parse(filePath);
                if (bootInfo.isValid) {
                    inspection.kernelRelease = bootInfo.kernelRelease;
                    inspection.osVersion = bootInfo.osVersion;
                    inspection.osPatchLevel = bootInfo.osPatchLevel;
                }
            } else if (filename == "vbmeta.img") {
                LOG_PARSER_INFO("Found vbmeta.img, parsing...");
                auto vbmetaInfo = VbmetaParser::parse(filePath);
                if (vbmetaInfo.isValid) {
                    inspection.avbEnabled = vbmetaInfo.avbEnabled;
                    inspection.avbRelease = vbmetaInfo.releaseString;
                }
            } else if (filename == "payload.bin") {
                LOG_PARSER_INFO("Found payload.bin, parsing...");
                hasPayload = true;
                auto payloadInfo = PayloadParser::parse(filePath);
                if (payloadInfo.isValid) {
                    inspection.logicalPartitions = payloadInfo.partitions;
                }
            } else if (filename == "super.img") {
                LOG_PARSER_INFO("Found super.img, checking magic...");
                if (SuperParser::isSuperImage(filePath)) {
                    hasSuper = true;
                }
            } else if (filename == "system.img" || filename == "vendor.img" || filename == "product.img") {
                LOG_PARSER_INFO("Found {}, validating filesystem...", filename);
                if (Ext4Parser::isExt4Image(filePath)) {
                    LOG_PARSER_INFO("{} filesystem validation succeeded.", filename);
                } else {
                    LOG_PARSER_WARN("{} filesystem magic check failed or is not EXT4.", filename);
                }
            }
        }
    }

    if (hasPayload) {
        inspection.partitionLayout = "A/B (OTA Payload)";
    } else if (hasSuper) {
        inspection.partitionLayout = "A/B (Dynamic Super Partition)";
    } else {
        inspection.partitionLayout = "A-only (Legacy Layout)";
    }

    LOG_PARSER_INFO("ROM Inspection Completed: OS={}, Kernel='{}', Layout={}", 
        inspection.osVersion, inspection.kernelRelease, inspection.partitionLayout);

    return inspection;
}
