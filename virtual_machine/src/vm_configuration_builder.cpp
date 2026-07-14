#include "virtual_machine/vm_configuration_builder.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

QemuConfigParameters VmConfigurationBuilder::build(
    const std::filesystem::path& romDir, 
    const VmProfile& profile,
    const std::string& tempKernelPath,
    const std::string& tempRamdiskPath
) {
    QemuConfigParameters params;
    params.cpu = profile.cpu;
    params.machine = profile.machine;
    params.memory = profile.memory;
    params.nographic = profile.nographic;
    params.kernelPath = tempKernelPath;
    params.ramdiskPath = tempRamdiskPath;

    std::filesystem::path configPath = romDir / "boot_config.json";
    if (std::filesystem::exists(configPath)) {
        try {
            std::ifstream in(configPath);
            nlohmann::json j;
            in >> j;

            LOG_BOOT_INFO("VmConfigurationBuilder: Mapping profile '{}' parameters using boot config v{}", 
                profile.name, j.value("version", 1));

            // Load extra VM configurations if specified in boot_config
            if (j.contains("vm_args")) {
                params.extraArgs = j.value("vm_args", "");
            }
            if (j.contains("cmdline")) {
                params.cmdline = j.value("cmdline", "");
            }
        } catch (const std::exception& e) {
            LOG_BOOT_ERROR("VmConfigurationBuilder: Failed to parse boot_config.json: {}", e.what());
        }
    } else {
        LOG_BOOT_WARN("VmConfigurationBuilder: boot_config.json missing in ROM directory. Building VM using profile fallbacks.");
    }

    return params;
}
