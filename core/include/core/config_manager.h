#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace core {

struct VmProfileData {
    std::string name;
    std::string cpu = "cortex-a53";
    std::string machine = "virt";
    std::string memory = "1024M";
    bool nographic = true;
};

class ConfigManager {
public:
    static std::filesystem::path getSettingsFilePath();
    static std::filesystem::path getProfilesFilePath();

    // Settings
    static std::string getQemuPath();
    static void setQemuPath(const std::string& path);

    // VM Profiles
    static std::vector<VmProfileData> loadProfiles();
    static VmProfileData getProfile(const std::string& name);

    // Initializer
    static void initializeDefaultConfigs();
};

} // namespace core
