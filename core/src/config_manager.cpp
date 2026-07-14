#include "core/config_manager.h"
#include "core/utils.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace core {

std::filesystem::path ConfigManager::getSettingsFilePath() {
    return getExecutableDir() / "settings.json";
}

std::filesystem::path ConfigManager::getProfilesFilePath() {
    return getExecutableDir() / "vm_profiles.json";
}

std::string ConfigManager::getQemuPath() {
    auto path = getSettingsFilePath();
    if (!std::filesystem::exists(path)) return "";

    try {
        std::ifstream in(path);
        nlohmann::json j;
        in >> j;
        return j.value("qemu_path", "");
    } catch (...) {
        return "";
    }
}

void ConfigManager::setQemuPath(const std::string& path) {
    auto filepath = getSettingsFilePath();
    nlohmann::json j;
    
    if (std::filesystem::exists(filepath)) {
        try {
            std::ifstream in(filepath);
            in >> j;
        } catch (...) {}
    }

    j["qemu_path"] = path;

    try {
        std::ofstream out(filepath);
        out << j.dump(4);
    } catch (...) {}
}

std::vector<VmProfileData> ConfigManager::loadProfiles() {
    initializeDefaultConfigs();

    std::vector<VmProfileData> list;
    auto filepath = getProfilesFilePath();
    
    try {
        std::ifstream in(filepath);
        nlohmann::json j;
        in >> j;

        if (j.contains("profiles")) {
            for (auto& [name, data] : j["profiles"].items()) {
                VmProfileData p;
                p.name = name;
                p.cpu = data.value("cpu", "cortex-a53");
                p.machine = data.value("machine", "virt");
                p.memory = data.value("memory", "1024M");
                p.nographic = data.value("nographic", true);
                list.push_back(p);
            }
        }
    } catch (const std::exception& e) {
        LOG_BOOT_ERROR("Failed to load vm_profiles.json: {}", e.what());
    }

    return list;
}

VmProfileData ConfigManager::getProfile(const std::string& name) {
    auto list = loadProfiles();
    for (const auto& p : list) {
        if (p.name == name) return p;
    }
    
    // Return default fallback
    VmProfileData p;
    p.name = name;
    return p;
}

void ConfigManager::initializeDefaultConfigs() {
    // 1. settings.json default
    auto settingsPath = getSettingsFilePath();
    if (!std::filesystem::exists(settingsPath)) {
        nlohmann::json j;
        j["qemu_path"] = "";
        std::ofstream out(settingsPath);
        if (out) {
            out << j.dump(4);
        }
    }

    // 2. vm_profiles.json default
    auto profilesPath = getProfilesFilePath();
    if (!std::filesystem::exists(profilesPath)) {
        nlohmann::json j;
        j["profiles"]["generic_arm64"] = {
            {"cpu", "cortex-a53"},
            {"machine", "virt"},
            {"memory", "1024M"},
            {"nographic", true}
        };
        // Add another profile for device targets
        j["profiles"]["violet"] = {
            {"cpu", "cortex-a76"},
            {"machine", "virt"},
            {"memory", "2048M"},
            {"nographic", true}
        };
        std::ofstream out(profilesPath);
        if (out) {
            out << j.dump(4);
            LOG_BOOT_INFO("Created default vm_profiles.json at: {}", profilesPath.string());
        }
    }
}

} // namespace core
