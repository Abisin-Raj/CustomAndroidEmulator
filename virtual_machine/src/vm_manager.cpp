#include "virtual_machine/vm_manager.h"
#include "virtual_machine/vm_profile.h"
#include "virtual_machine/vm_configuration_builder.h"
#include "boot_assets/boot_asset_loader.h"
#include "qemu_backend/qemu_detector.h"
#include "qemu_backend/qemu_arguments.h"
#include "qemu_backend/qemu_launcher.h"
#include "qemu_backend/smc_interceptor.h"
#include "boot_events/event_dispatcher.h"
#include "boot_pipeline/boot_pipeline_manager.h"
#include "kernel_runtime/kernel_commandline.h"
#include "firmware/smc_dispatcher.h"
#include "firmware/scm_services.h"
#include "core/logger.h"
#include "core/utils.h"
#include "behavior_exploration_tool/bet_system.h"
#include <nlohmann/json.hpp>
#include <fstream>

VmManager::VmManager(QObject* parent)
    : QObject(parent)
    , m_launcher(new QemuLauncher(this))
    , m_dispatcher(new EventDispatcher(this))
    , m_pipelineManager(new BootPipelineManager(m_dispatcher))
    , m_smcDispatcher(new firmware::SmcDispatcher())
    , m_isRunning(false) {
    m_pipelineManager->initializeDefaultDetectors();

    // Register QCOM boot SCM services (handles BOOT and INFO requests)
    m_smcDispatcher->registerService(std::make_unique<firmware::QcomBootService>());
    m_smcDispatcher->registerService(std::make_unique<firmware::QcomInfoService>());

    connect(m_launcher, &QemuLauncher::stateChanged, this, &VmManager::onLauncherStateChanged);
    connect(m_launcher, &QemuLauncher::finished, this, &VmManager::onLauncherFinished);
    connect(m_launcher, &QemuLauncher::logLineRead, this, &VmManager::onLogLineRead);
}

VmManager::~VmManager() {
    stopVm();
    delete m_pipelineManager;
    delete m_smcDispatcher;
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

    // Build original command line arguments before console args replacement
    QStringList originalArgs = QemuArguments::build(params);
    std::string originalFullCmd = qemuPath;
    for (const auto& arg : originalArgs) {
        originalFullCmd += " " + arg.toStdString();
    }

    // Apply kernel runtime commandline improvements
    params.cmdline = KernelCommandLine::build(params.cmdline);

    QStringList args = QemuArguments::build(params);

    // Reset boot pipeline context and session configurations
    m_pipelineManager->setRomName(romName);
    m_pipelineManager->loadSymbols(romDir.string());
    m_pipelineManager->getSession() = BootSession();
    m_pipelineManager->getSession().setRomName(romName);
    m_pipelineManager->getSession().setOriginalQemuCommandLine(originalFullCmd);

    std::string fullCmd = qemuPath;
    for (const auto& arg : args) {
        fullCmd += " " + arg.toStdString();
    }
    m_pipelineManager->getSession().setQemuCommandLine(fullCmd);

    m_fullLogText.clear();
    m_activeRomName = romName;
    m_activeRomDir = romDir;

    // 5. Configure GDB stub for SMC interception (if symbols are available)
    // The interceptor requires the QCOM SCM function to be in the resolver.
    bool interceptEnabled = false;
    {
        const kernel_analysis::SymbolResolver& resolver =
            m_pipelineManager->getResolver();
        uint64_t scmFuncAddr = resolver.findSymbolAddress("qcom_scm_set_warm_boot_addr");
        if (scmFuncAddr != 0) {
            constexpr quint16 GDB_PORT = 1238;

            m_launcher->enableGdbStub(GDB_PORT);

            // Destroy any previous interceptor instance
            if (m_smcInterceptor) {
                m_smcInterceptor->stop();
                delete m_smcInterceptor;
                m_smcInterceptor = nullptr;
            }

            m_smcInterceptor = new qemu_backend::SmcInterceptor(
                *m_smcDispatcher, resolver, GDB_PORT, this);

            connect(m_smcInterceptor, &qemu_backend::SmcInterceptor::smcIntercepted,
                [this](quint64 smcId, bool handled, quint64 returnX0) {
                    LOG_QEMU_INFO("VmManager: SMC 0x{:08x} intercepted — handled={} X0=0x{:x}",
                        smcId, handled, returnX0);
                });
            interceptEnabled = true;
            LOG_QEMU_INFO("VmManager: SMC interception enabled — "
                "qcom_scm_set_warm_boot_addr resolved at 0x{:016x}", scmFuncAddr);
        } else {
            LOG_QEMU_WARN("VmManager: qcom_scm_set_warm_boot_addr not found in System.map. "
                "Running without SMC interception.");
        }
    }

    // Initialize and start Behavior Exploration Tool (BET) session
    std::filesystem::path betDbDir = core::getExecutableDir() / "logs" / "bet";
    bet::BetSystem::instance().initialize(betDbDir.string());
    bet::BetSystem::instance().startSession(romName, "5.4.x", "1.0.0");

    // 6. Spawn QEMU
    m_timer.start();
    if (!m_launcher->start(qemuPath, args)) {
        LOG_QEMU_ERROR("VmManager: Process launch failed. Cleaning up temporary folders...");
        BootAssetLoader::cleanTempAssets(tempDir);
        return false;
    }

    // 7. Start SMC interceptor thread (connects to QEMU's GDB stub)
    if (interceptEnabled && m_smcInterceptor) {
        if (!m_smcInterceptor->start()) {
            LOG_QEMU_WARN("VmManager: SMC interceptor failed to start. "
                "QEMU may hang at the warm-boot SCM call.");
        }
    }

    m_isRunning = true;
    emit vmStateChanged(true);
    m_dispatcher->dispatchStateChange(BootState::VM_STARTING);
    return true;
}

void VmManager::stopVm() {
    if (m_smcInterceptor) {
        m_smcInterceptor->stop();
    }
    m_launcher->stop();
}

bool VmManager::isVmRunning() const {
    return m_isRunning && m_launcher->isRunning();
}

void VmManager::onLogLineRead(const QString& line) {
    double elapsed = m_timer.elapsed() / 1000.0;
    std::string lineStr = line.toStdString();
    bet::BetSystem::instance().pushLogLine(lineStr);
    
    std::string annotated = m_pipelineManager->processLogLine(lineStr, elapsed);
    m_fullLogText += QString::fromStdString(annotated) + "\n";
}

void VmManager::onLauncherStateChanged(bool running) {
    if (!running) {
        LOG_QEMU_INFO("VmManager: VM execution finished. Releasing active session locks...");
        
        // Export boot session statistics logs
        std::filesystem::path logsDir = core::getExecutableDir() / "logs";
        
        // Populate final session results and metadata
        double elapsed = m_timer.elapsed() / 1000.0;
        m_pipelineManager->getSession().setDuration(elapsed);
        
        BootState finalState = m_pipelineManager->getContext().currentState;
        std::string stateStr = "VM_STARTING";
        switch (finalState) {
            case BootState::VM_STARTING:         stateStr = "VM_STARTING"; break;
            case BootState::KERNEL_LOADING:      stateStr = "KERNEL_LOADING"; break;
            case BootState::KERNEL_RUNNING:      stateStr = "KERNEL_RUNNING"; break;
            case BootState::USERSPACE_STARTING:  stateStr = "USERSPACE_STARTING"; break;
            case BootState::SYSTEM_SERVICES:     stateStr = "SYSTEM_SERVICES"; break;
            case BootState::BOOT_COMPLETE:       stateStr = "BOOT_COMPLETE"; break;
            case BootState::EMULATION_FAILED:     stateStr = "EMULATION_FAILED"; break;
        }
        m_pipelineManager->getSession().setLastState(stateStr);

        std::string outcome = "Unknown";
        if (finalState == BootState::BOOT_COMPLETE) {
            outcome = "Success";
        } else if (m_pipelineManager->getSession().getPanicCount() > 0) {
            outcome = "Kernel Panic";
        } else {
            outcome = "Failed / Closed early";
        }
        m_pipelineManager->getSession().setFinalResult(outcome);

        std::string panicReason = "";
        for (const auto& ev : m_pipelineManager->getSession().getTimeline()) {
            if (ev.type == BootEventType::KernelPanic) {
                panicReason = ev.details;
                if (ev.metadata.count("suggested_cause")) {
                    panicReason += " (" + ev.metadata.at("suggested_cause") + ")";
                }
                break;
            }
        }
        m_pipelineManager->getSession().setPanicReason(panicReason);

        m_pipelineManager->getSession().writeLogFile(logsDir.string(), m_fullLogText.toStdString());
        m_pipelineManager->getSession().writeJsonSummary(logsDir.string());
        m_pipelineManager->getSession().writeQemuCmdFile(logsDir.string());

        // End Behavior Exploration Tool (BET) session
        bet::BetSystem::instance().endSession(outcome);

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
