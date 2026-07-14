#include "core/logger.h"
#include "image_parser/bootimg_parser.h"
#include "image_parser/vbmeta_parser.h"
#include "image_parser/ext4_parser.h"
#include "boot_manager/boot_manager.h"
#include "rom_manager/rom_manager.h"
#include "models/rom_metadata.h"
#include "core/config_manager.h"
#include "boot_assets/boot_asset_loader.h"
#include "virtual_machine/vm_profile.h"
#include "qemu_backend/qemu_arguments.h"
#include "boot_events/event_types.h"
#include "boot_events/event_dispatcher.h"
#include "boot_session/boot_session.h"
#include "kernel_runtime/kernel_commandline.h"
#include "boot_pipeline/boot_pipeline_manager.h"
#include "boot_pipeline/kernel_detector.h"
#include "boot_pipeline/init_detector.h"
#include "boot_pipeline/panic_detector.h"
#include <iostream>
#include <fstream>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <nlohmann/json.hpp>

// A simple macro for reporting assert successes
#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        std::cerr << "Assertion failed: " << msg << " (" << #cond << ")\n"; \
        std::exit(1); \
    } else { \
        std::cout << "[PASS] " << msg << "\n"; \
    }

void testBootImgParser() {
    std::filesystem::path bootPath = "test_boot.img";
    
    // Construct mock boot.img with version 2 header
    std::vector<char> header(2048, 0);
    std::memcpy(header.data(), "ANDROID!", 8);
    
    uint32_t kernelSize = 120;
    uint32_t ramdiskSize = 450;
    uint32_t pageSize = 2048;
    uint32_t headerVersion = 2;
    uint32_t osVal = (13 << 25) | (0 << 18) | (0 << 11) | (23 << 4) | 6; // OS 13.0.0, 2023-06

    std::memcpy(header.data() + 8, &kernelSize, 4);
    std::memcpy(header.data() + 16, &ramdiskSize, 4);
    std::memcpy(header.data() + 36, &pageSize, 4);
    std::memcpy(header.data() + 40, &headerVersion, 4);
    std::memcpy(header.data() + 44, &osVal, 4);

    std::string cmdline = "console=ttyMSM0 androidboot.hardware=violet";
    std::memcpy(header.data() + 64, cmdline.c_str(), cmdline.length());

    std::ofstream out(bootPath, std::ios::binary);
    out.write(header.data(), header.size());
    out.close();

    // Parse and assert
    auto info = BootImgParser::parse(bootPath);
    std::filesystem::remove(bootPath);

    TEST_ASSERT(info.isValid, "Boot image must be parsed as valid");
    TEST_ASSERT(info.headerVersion == 2, "Header version must be 2");
    TEST_ASSERT(info.pageSize == 2048, "Page size must be 2048");
    TEST_ASSERT(info.kernelSize == 120, "Kernel size must be 120");
    TEST_ASSERT(info.ramdiskSize == 450, "Ramdisk size must be 450");
    TEST_ASSERT(info.osVersion == "13.0.0", "OS version must decode to 13.0.0");
    TEST_ASSERT(info.osPatchLevel == "2023-06", "OS patch level must decode to 2023-06");
    TEST_ASSERT(info.cmdline.find("console=ttyMSM0") != std::string::npos, "Cmdline must match mock strings");
    
    // Check offsets calculation
    TEST_ASSERT(info.getKernelOffset() == 2048, "Kernel offset must equal page size");
    TEST_ASSERT(info.getRamdiskOffset() == 4096, "Ramdisk offset must align kernel bounds correctly");
}

void testVbmetaParser() {
    std::filesystem::path vbPath = "test_vbmeta.img";
    std::vector<char> header(256, 0);
    
    // Magic AVB0
    std::memcpy(header.data(), "AVB0", 4);
    
    // Rollback index (offset 112, 8 bytes, uint64_t BE)
    // 00 00 00 00 00 00 00 05
    header[119] = 0x05;
    
    // Flags (offset 120, 4 bytes, uint32_t BE) - set AVB_VBMETA_IMAGE_FLAGS_VERIFICATION_DISABLED (1 << 1)
    // 00 00 00 02
    header[123] = 0x02;

    std::string release = "avbtool 1.2.0";
    std::memcpy(header.data() + 128, release.c_str(), release.length());

    std::ofstream out(vbPath, std::ios::binary);
    out.write(header.data(), header.size());
    out.close();

    auto info = VbmetaParser::parse(vbPath);
    std::filesystem::remove(vbPath);

    TEST_ASSERT(info.isValid, "VBMeta image must be parsed as valid");
    TEST_ASSERT(info.rollbackIndex == 5, "Rollback index must equal 5");
    TEST_ASSERT(!info.avbEnabled, "AVB must be disabled when verification flag is set");
    TEST_ASSERT(info.releaseString == "avbtool 1.2.0", "Release string must match");
}

void testExt4Parser() {
    std::filesystem::path ext4Path = "test_system.img";
    std::vector<char> rawFs(2048, 0);
    
    // EXT4 magic EF53 at offset 1080 (little endian)
    rawFs[1080] = 0x53;
    rawFs[1081] = 0xEF;

    std::ofstream out(ext4Path, std::ios::binary);
    out.write(rawFs.data(), rawFs.size());
    out.close();

    bool isExt4 = Ext4Parser::isExt4Image(ext4Path);
    std::filesystem::remove(ext4Path);

    TEST_ASSERT(isExt4, "System image with ext4 magic must be validated");
}

void testBootManagerValidation() {
    std::filesystem::path romDir = "test_rom_folder";
    std::filesystem::create_directories(romDir);

    // 1. Create mock boot.img
    std::vector<char> bootHeader(2048, 0);
    std::memcpy(bootHeader.data(), "ANDROID!", 8);
    uint32_t kernelSize = 100;
    uint32_t ramdiskSize = 200;
    uint32_t pageSize = 2048;
    std::memcpy(bootHeader.data() + 8, &kernelSize, 4);
    std::memcpy(bootHeader.data() + 16, &ramdiskSize, 4);
    std::memcpy(bootHeader.data() + 36, &pageSize, 4);
    std::ofstream outBoot(romDir / "boot.img", std::ios::binary);
    outBoot.write(bootHeader.data(), bootHeader.size());
    outBoot.close();

    // 2. Create mock system.img (EXT4)
    std::vector<char> systemFs(2048, 0);
    systemFs[1080] = 0x53;
    systemFs[1081] = 0xEF;
    std::ofstream outSys(romDir / "system.img", std::ios::binary);
    outSys.write(systemFs.data(), systemFs.size());
    outSys.close();

    // Run validate and prepare
    auto report = BootManager::validateAndPrepareBoot("mock_rom_test", romDir);

    TEST_ASSERT(report.status == "Warning", "ROM must be validated as Warning (due to missing vbmeta.img)");
    TEST_ASSERT(report.errors.empty(), "There should be no blocker errors");
    TEST_ASSERT(report.warnings.size() > 0, "Warnings list must contain missing vbmeta warning");

    // Check if boot_config.json is generated
    std::filesystem::path configPath = romDir / "boot_config.json";
    TEST_ASSERT(std::filesystem::exists(configPath), "boot_config.json descriptor must be written to ROM directory");

    // Read config JSON and assert properties
    std::ifstream in(configPath);
    nlohmann::json config;
    in >> config;
    in.close();

    TEST_ASSERT(config.value("version", 0) == 1, "Config version must equal 1");
    TEST_ASSERT(config.value("kernel_offset", 0ULL) == 2048ULL, "Kernel offset must match boot header page size");
    TEST_ASSERT(config.value("ramdisk_offset", 0ULL) == 4096ULL, "Ramdisk offset must match calculated bounds");
    TEST_ASSERT(config.value("vm_profile", "") == "generic_arm64", "VM profile must equal generic_arm64");

    // Cleanup
    std::filesystem::remove_all(romDir);
}

void testConfigManager() {
    std::string original = core::ConfigManager::getQemuPath();
    core::ConfigManager::setQemuPath("C:\\dummy_qemu_path\\qemu-system-aarch64.exe");
    TEST_ASSERT(core::ConfigManager::getQemuPath() == "C:\\dummy_qemu_path\\qemu-system-aarch64.exe", "Custom QEMU path must be saved and loaded");
    
    // Restore original path
    core::ConfigManager::setQemuPath(original);

    auto profiles = core::ConfigManager::loadProfiles();
    TEST_ASSERT(!profiles.empty(), "ConfigManager must load template hardware profiles");
    bool hasViolet = false;
    for (const auto& p : profiles) {
        if (p.name == "violet") hasViolet = true;
    }
    TEST_ASSERT(hasViolet, "Default profiles must contain violet");
}

void testBootAssetLoader() {
    std::filesystem::path dummyImg = "dummy_boot.img";
    std::vector<char> buffer(1000, 0);
    // Write distinct pattern at offset 200
    std::string pattern = "AndroidKernelSegment";
    std::memcpy(buffer.data() + 200, pattern.c_str(), pattern.length());

    std::ofstream out(dummyImg, std::ios::binary);
    out.write(buffer.data(), buffer.size());
    out.close();

    // Load segment via BootAssetLoader
    auto loaded = BootAssetLoader::loadBuffer(dummyImg, 200, pattern.length());
    std::filesystem::remove(dummyImg);

    TEST_ASSERT(loaded.size() == pattern.length(), "Loader must load the requested size");
    std::string loadedStr(loaded.begin(), loaded.end());
    TEST_ASSERT(loadedStr == pattern, "Loaded buffer content must match matching pattern");
}

void testVmProfile() {
    VmProfile p = VmProfile::loadByName("violet");
    TEST_ASSERT(p.cpu == "cortex-a76", "Violet profile CPU must map to cortex-a76");
    TEST_ASSERT(p.machine == "virt", "Violet profile machine board must map to virt");
    TEST_ASSERT(p.memory == "2048M", "Violet profile RAM size must map to 2048M");
}

void testQemuArguments() {
    QemuConfigParameters params;
    params.cpu = "cortex-a53";
    params.machine = "virt";
    params.memory = "1024M";
    params.kernelPath = "C:\\path\\kernel";
    params.ramdiskPath = "C:\\path\\ramdisk.img";
    params.nographic = true;

    QStringList args = QemuArguments::build(params);
    TEST_ASSERT(args.contains("-cpu"), "Arguments must contain -cpu option");
    TEST_ASSERT(args.contains("cortex-a53"), "Arguments must contain cortex-a53 value");
    TEST_ASSERT(args.contains("-M"), "Arguments must contain -M machine option");
    TEST_ASSERT(args.contains("-kernel"), "Arguments must contain -kernel");
}

void testKernelCommandLine() {
    std::string base = "console=ttyS0 quiet";
    std::string build = KernelCommandLine::build(base);
    TEST_ASSERT(build.find("console=ttyAMA0") == std::string::npos, "Should respect existing console setting if console= is present");
    TEST_ASSERT(build.find("earlycon=pl011,0x09000000") != std::string::npos, "Should append earlycon argument");
    TEST_ASSERT(build.find("printk.devkmsg=on") != std::string::npos, "Should append printk argument");

    std::string emptyBase = "";
    std::string builtEmpty = KernelCommandLine::build(emptyBase);
    TEST_ASSERT(builtEmpty.find("console=ttyAMA0") != std::string::npos, "Should add console argument if base is empty");
}

void testEventDispatcher() {
    EventDispatcher dispatcher;
    bool eventFired = false;
    bool stateFired = false;

    QObject::connect(&dispatcher, &EventDispatcher::eventDispatched, [&eventFired](const BootEvent& ev) {
        if (ev.type == BootEventType::CpuOnline) {
            eventFired = true;
        }
    });

    QObject::connect(&dispatcher, &EventDispatcher::stateChanged, [&stateFired](BootState state) {
        if (state == BootState::KERNEL_RUNNING) {
            stateFired = true;
        }
    });

    BootEvent ev;
    ev.type = BootEventType::CpuOnline;
    ev.severity = EventSeverity::Info;
    ev.details = "CPU0 online";
    ev.confidence = 1.0;
    ev.elapsedSeconds = 0.5;

    dispatcher.dispatchEvent(ev);
    dispatcher.dispatchStateChange(BootState::KERNEL_RUNNING);

    TEST_ASSERT(eventFired, "eventDispatched signal should fire successfully");
    TEST_ASSERT(stateFired, "stateChanged signal should fire successfully");
}

void testPanicDetector() {
    PanicDetector detector;
    DetectorContext ctx;
    ctx.elapsedSeconds = 2.4;

    std::string line1 = "[    2.400000] CPU: 0 PID: 1 Comm: swapper/0 Not tainted";
    auto events1 = detector.processLine(line1, ctx);
    TEST_ASSERT(events1.empty(), "Metadata collection should not trigger panic event immediately");

    std::string line2 = "[    2.405000] Kernel panic - not syncing: VFS: Unable to mount root fs on unknown-block(0,0)";
    auto events2 = detector.processLine(line2, ctx);
    TEST_ASSERT(events2.size() == 1, "Panic line must trigger panic event");
    TEST_ASSERT(events2[0].type == BootEventType::KernelPanic, "Event type must be KernelPanic");
    TEST_ASSERT(events2[0].severity == EventSeverity::Critical, "Severity must be Critical");
    TEST_ASSERT(events2[0].metadata["pid"] == "1", "PID metadata should be parsed successfully");
    TEST_ASSERT(events2[0].metadata["process"] == "swapper/0", "Process metadata should be parsed successfully");
    TEST_ASSERT(events2[0].metadata["suggested_cause"].find("Missing root filesystem") != std::string::npos, "Panic classification suggest cause correctly");
}

void testBootPipelineManager() {
    EventDispatcher dispatcher;
    BootPipelineManager manager(&dispatcher);
    manager.initializeDefaultDetectors();
    manager.setRomName("test_pipeline_rom");

    // 1. Initial State
    TEST_ASSERT(manager.getContext().currentState == BootState::VM_STARTING, "Initial state should be VM_STARTING");

    // 2. Feed Linux Version log
    manager.processLogLine("[    0.050000] Linux version 4.19.191-gki (android@google.com)", 0.05);
    TEST_ASSERT(manager.getContext().currentState == BootState::KERNEL_RUNNING, "State should transition to KERNEL_RUNNING");
    TEST_ASSERT(manager.getContext().kernelVersion.find("Linux version 4.19.191-gki") != std::string::npos, "Should record kernel version");

    // 3. Feed Init Started
    manager.processLogLine("[    1.200000] init: init first stage started!", 1.20);
    TEST_ASSERT(manager.getContext().currentState == BootState::USERSPACE_STARTING, "State should transition to USERSPACE_STARTING");

    // 4. Feed Property Completed
    manager.processLogLine("[    4.500000] init: property_set(\"sys.boot_completed\", \"1\")", 4.50);
    TEST_ASSERT(manager.getContext().currentState == BootState::BOOT_COMPLETE, "State should transition to BOOT_COMPLETE");

    // Verify session statistics export
    auto stats = manager.getSession().generateStats();
    TEST_ASSERT(stats.find("Boot Duration:     4.50") != std::string::npos, "Stats duration must match");
    
    manager.getSession().setLastState("BOOT_COMPLETE");
    TEST_ASSERT(manager.getSession().getLastState() == "BOOT_COMPLETE", "Last state must be BOOT_COMPLETE");

    manager.getSession().setPanicReason("VFS panic");
    TEST_ASSERT(manager.getSession().getPanicReason() == "VFS panic", "Panic reason must be VFS panic");

    manager.getSession().setFinalResult("Success");
    manager.getSession().setDuration(4.50);
    manager.getSession().writeJsonSummary("test_logs_dir");

    manager.getSession().setQemuCommandLine("qemu-system-aarch64 -cpu cortex-a53");
    TEST_ASSERT(manager.getSession().getQemuCommandLine() == "qemu-system-aarch64 -cpu cortex-a53", "Qemu cmdline must match");
    manager.getSession().writeQemuCmdFile("test_logs_dir");
    
    // Ensure files exist
    bool jsonFileFound = false;
    bool qemuFileFound = false;
    for (const auto& entry : std::filesystem::directory_iterator("test_logs_dir")) {
        if (entry.path().extension() == ".json") {
            jsonFileFound = true;
        }
        if (entry.path().string().find(".qemu.txt") != std::string::npos) {
            qemuFileFound = true;
        }
    }
    TEST_ASSERT(jsonFileFound, "JSON summary log file must be exported");
    TEST_ASSERT(qemuFileFound, "QEMU command line file must be exported");
    std::filesystem::remove_all("test_logs_dir");
}

int main() {
    core::initLogger();
    std::cout << "======================================\n";
    std::cout << "Starting CustomAndroidEmulator Tests\n";
    std::cout << "======================================\n";

    testBootImgParser();
    testVbmetaParser();
    testExt4Parser();
    testBootManagerValidation();
    testConfigManager();
    testBootAssetLoader();
    testVmProfile();
    testQemuArguments();
    testKernelCommandLine();
    testEventDispatcher();
    testPanicDetector();
    testBootPipelineManager();

    std::filesystem::path realRom = "PixelExperience_violet-13.0-20240127-0711-OFFICIAL.zip";
    if (std::filesystem::exists(realRom)) {
        std::cout << "\n======================================\n";
        std::cout << "Testing real ROM file: " << realRom.filename().string() << "\n";
        std::cout << "======================================\n";
        RomManager manager("test_real_rom_dir");
        RomMetadata meta;
        std::string error;
        bool valid = manager.validateRomZip(realRom.string(), meta, error);
        if (valid) {
            std::cout << "[SUCCESS] ROM is valid and recognizable!\n";
            std::cout << "  Name:            " << meta.name << "\n";
            std::cout << "  Device Model:    " << meta.device << "\n";
            std::cout << "  Codename/Board:  " << meta.codename << "\n";
            std::cout << "  Android Version: " << meta.androidVersion << "\n";
            std::cout << "  Architecture:    " << meta.architecture << "\n";
            std::cout << "  Status:          " << meta.status << "\n";
        } else {
            std::cout << "[FAIL] ROM is not recognized as valid!\n";
            std::cout << "  Error: " << error << "\n";
        }
        std::filesystem::remove_all("test_real_rom_dir");
        std::cout << "======================================\n";
    }

    std::cout << "======================================\n";
    std::cout << "All unit tests completed successfully!\n";
    std::cout << "======================================\n";
    return 0;
}
