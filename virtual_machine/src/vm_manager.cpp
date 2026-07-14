#include "virtual_machine/vm_manager.h"
#include "virtual_machine/vm_profile.h"
#include "virtual_machine/vm_configuration_builder.h"
#include "boot_assets/boot_asset_loader.h"
#include "qemu_backend/qemu_detector.h"
#include "qemu_backend/qemu_arguments.h"
#include "qemu_backend/qemu_launcher.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

VmManager::VmManager(QObject* parent)
    : QObject(parent)
    , m_launcher(new QemuLauncher(this))
    , m_isRunning(false) {
    connect(m_launcher, &QemuLauncher::stateChanged, this, &VmManager::onLauncherStateChanged);
    connect(m_launcher, &QemuLauncher::finished, this, &VmManager::onLauncherFinished);
}

VmManager::~VmManager() {
    stopVm();
}

bool VmManager::startVm(const std::string& romName, const std::filesystem::path& romDir) {
    if (isVmRunning()) {
        LOG_QEMU_WARN("VmManager: Cannot launch ROM '{}'. A virtual machine is already executing.", romName);
        return false;
    }

    LOG_QEMU_INFO("VmManager: Initiating virtual machine runtime for ROM '{}'", romName);

    // 1. Locate QEMU system binary
    std::string qemuPath = QemuDetector::detectQemuPath();
    if (qemuPath.empty()) {
        LOG_QEMU_ERROR("VmManager: Launch failed. QEMU system binary (qemu-system-aarch64) is not installed or configured.");
        return false;
    }

    // 2. Read boot config parameters
    std::filesystem::path configPath = romDir / "boot_config.json";
    if (!std::filesystem::exists(configPath)) {
        LOG_QEMU_ERROR("VmManager: Launch failed. ROM directory is missing boot_config.json validation parameters.");
        return false;
    }

    uint64_t kernelOffset = 0, kernelSize = 0;
    uint64_t ramdiskOffset = 0, ramdiskSize = 0;
    std::string profileName = "generic_arm64";

    try {
        std::ifstream in(configPath);
        nlohmann::json j;
        in >> j;
        kernelOffset = j.value("kernel_offset", 0ULL);
        kernelSize = j.value("kernel_size", 0ULL);
        ramdiskOffset = j.value("ramdisk_offset", 0ULL);
        ramdiskSize = j.value("ramdisk_size", 0ULL);
        profileName = j.value("vm_profile", "generic_arm64");
    } catch (const std::exception& e) {
        LOG_QEMU_ERROR("VmManager: Failed to read boot config values: {}", e.what());
        return false;
    }

    // 3. Extract boot assets in memory and write temporary boot files
    std::filesystem::path bootImgPath = romDir / "boot.img";
    if (!std::filesystem::exists(bootImgPath)) {
        LOG_QEMU_ERROR("VmManager: Launch failed. boot.img not found in: {}", bootImgPath.string());
        return false;
    }

    LOG_QEMU_INFO("VmManager: Carving guest boot assets in memory...");
    std::vector<char> kernelBuffer = BootAssetLoader::loadBuffer(bootImgPath, kernelOffset, kernelSize);
    std::vector<char> ramdiskBuffer = BootAssetLoader::loadBuffer(bootImgPath, ramdiskOffset, ramdiskSize);

    if (kernelBuffer.empty()) {
        LOG_QEMU_ERROR("VmManager: Failed to load kernel bytes from boot.img.");
        return false;
    }

    std::filesystem::path tempDir = romDir / "_tmp_boot";
    std::filesystem::path tempKernelPath = tempDir / "kernel";
    std::filesystem::path tempRamdiskPath = tempDir / "ramdisk.img";

    LOG_QEMU_INFO("VmManager: Preparing temporary executable directory layout in: {}", tempDir.string());
    std::filesystem::create_directories(tempDir);

    if (!BootAssetLoader::writeTempFile(kernelBuffer, tempKernelPath)) {
        LOG_QEMU_ERROR("VmManager: Failed to write kernel to temporary boot assets.");
        BootAssetLoader::cleanTempAssets(tempDir);
        return false;
    }

    bool hasRamdisk = !ramdiskBuffer.empty();
    if (hasRamdisk) {
        if (!BootAssetLoader::writeTempFile(ramdiskBuffer, tempRamdiskPath)) {
            LOG_QEMU_WARN("VmManager: Failed to write ramdisk to temporary boot assets. Attempting raw kernel boot.");
            hasRamdisk = false;
        }
    }

    // 4. Load hardware profile and build VM arguments
    VmProfile profile = VmProfile::loadByName(profileName);
    QemuConfigParameters params = VmConfigurationBuilder::build(
        romDir, 
        profile, 
        tempKernelPath.string(), 
        hasRamdisk ? tempRamdiskPath.string() : ""
    );

    QStringList args = QemuArguments::build(params);

    // 5. Spawn QEMU
    m_activeRomDir = romDir;
    if (!m_launcher->start(qemuPath, args)) {
        LOG_QEMU_ERROR("VmManager: Process launch failed. Cleaning up temporary folders...");
        BootAssetLoader::cleanTempAssets(tempDir);
        return false;
    }

    m_isRunning = true;
    emit vmStateChanged(true);
    return true;
}

void VmManager::stopVm() {
    m_launcher->stop();
}

bool VmManager::isVmRunning() const {
    return m_isRunning && m_launcher->isRunning();
}

void VmManager::onLauncherStateChanged(bool running) {
    if (!running) {
        LOG_QEMU_INFO("VmManager: VM execution finished. Releasing active session locks...");
        
        // Clean up temporary boot files
        if (!m_activeRomDir.empty()) {
            std::filesystem::path tempDir = m_activeRomDir / "_tmp_boot";
            BootAssetLoader::cleanTempAssets(tempDir);
            m_activeRomDir.clear();
        }

        m_isRunning = false;
        emit vmStateChanged(false);
    }
}

void VmManager::onLauncherFinished(int exitCode) {
    emit vmFinished(exitCode);
}
