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

    // 2. Search executable in system PATH (using Qt portable helper)
    QString envPath = QStandardPaths::findExecutable("qemu-system-aarch64");
    if (!envPath.isEmpty()) {
        std::string pathStr = envPath.toStdString();
        LOG_QEMU_DEBUG("QemuDetector: Found QEMU in system PATH: {}", pathStr);
        return pathStr;
    }

    // 3. Scan common paths on Windows
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
