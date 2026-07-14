#pragma once
#include <string>

struct VmProfile {
    std::string name = "generic_arm64";
    std::string cpu = "cortex-a53";
    std::string machine = "virt";
    std::string memory = "1024M";
    bool nographic = true;

    /**
     * @brief Load a specific hardware profile by name using core::ConfigManager.
     */
    static VmProfile loadByName(const std::string& name);
};
