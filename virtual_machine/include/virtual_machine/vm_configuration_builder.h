#pragma once
#include "virtual_machine/vm_profile.h"
#include "qemu_backend/qemu_arguments.h"
#include <string>
#include <filesystem>

class VmConfigurationBuilder {
public:
    /**
     * @brief Transforms boot_config.json offsets and dynamic profiles into QEMU configuration parameters.
     */
    static QemuConfigParameters build(
        const std::filesystem::path& romDir, 
        const VmProfile& profile,
        const std::string& tempKernelPath,
        const std::string& tempRamdiskPath
    );
};
