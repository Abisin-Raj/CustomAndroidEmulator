#include "qemu_backend/qemu_detector.h"
#include "core/config_manager.h"
#include "core/logger.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <filesystem>
#include <vector>

std::string QemuDetector::detectQemuPath() {
    // 1. Check custom path in settings
    std::string configPath = core::ConfigManager::getQemuPath();
    if (!configPath.empty() && QFileInfo(QString::fromStdString(configPath)).exists()) {
        LOG_QEMU_DEBUG("QemuDetector: Found custom QEMU path in settings: {}", configPath);
        return configPath;
    }

    // 2. Check custom compiled QEMU binary
    std::string customQemuPath = "C:\\Users\\ABISIN RAJ\\Desktop\\Projects\\CustomAndroidQEMU\\build\\qemu-system-aarch64.exe";
    if (std::filesystem::exists(customQemuPath)) {
        LOG_QEMU_INFO("QemuDetector: Prioritizing CustomAndroidQEMU: {}", customQemuPath);
        return customQemuPath;
    }
    std::string customBuildPath = "C:\\qemu_source\\build\\qemu-system-aarch64.exe";
    if (std::filesystem::exists(customBuildPath)) {
        LOG_QEMU_INFO("QemuDetector: Prioritizing custom compiled QEMU: {}", customBuildPath);
        return customBuildPath;
    }

    // 3. Search executable in system PATH
    QString envPath = QStandardPaths::findExecutable("qemu-system-aarch64");
    if (!envPath.isEmpty()) {
        std::string pathStr = envPath.toStdString();
        LOG_QEMU_DEBUG("QemuDetector: Found QEMU in system PATH: {}", pathStr);
        return pathStr;
    }

    // 4. Scan common fallback paths on Windows
    std::vector<std::string> candidates = {
        "C:\\Program Files\\qemu\\qemu-system-aarch64.exe",
        "C:\\Program Files (x86)\\qemu\\qemu-system-aarch64.exe",
        "C:\\msys64\\mingw64\\bin\\qemu-system-aarch64.exe",
        "C:\\msys64\\usr\\bin\\qemu-system-aarch64.exe"
    };


    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            LOG_QEMU_DEBUG("QemuDetector: Detected standard QEMU path: {}", path);
            return path;
        }
    }

    LOG_QEMU_WARN("QemuDetector: Could not detect qemu-system-aarch64.exe on system.");
    return "";
}
