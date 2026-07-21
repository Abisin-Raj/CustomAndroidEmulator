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
#include "image_converter/brotli_decompressor.h"
#include "image_converter/transfer_list_parser.h"
#include "image_converter/dat_image_builder.h"
#include "kernel_analysis/symbol_resolver.h"
#include "firmware/smc_types.h"
#include "firmware/smc_dispatcher.h"
#include "firmware/scm_services.h"
#include <QCoreApplication>
#include <QTimer>
#include "virtual_machine/vm_manager.h"
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
    // Device-specific console should be replaced with QEMU PL011 equivalent
    std::string base = "console=ttyMSM0,115200n8 earlycon=msm_geni_serial,0x880000 quiet";
    std::string build = KernelCommandLine::build(base);
    TEST_ASSERT(build.find("console=ttyAMA0") != std::string::npos, "Should replace device console with ttyAMA0");
    TEST_ASSERT(build.find("ttyMSM0") == std::string::npos, "Should remove Qualcomm UART console arg");
    TEST_ASSERT(build.find("earlycon=pl011,0x09000000") != std::string::npos, "Should replace earlycon with PL011");
    TEST_ASSERT(build.find("msm_geni_serial") == std::string::npos, "Should remove Qualcomm earlycon arg");
    TEST_ASSERT(build.find("printk.devkmsg=on") != std::string::npos, "Should append printk argument");
    TEST_ASSERT(build.find("quiet") != std::string::npos, "Should preserve other kernel args");

    std::string emptyBase = "";
    std::string builtEmpty = KernelCommandLine::build(emptyBase);
    TEST_ASSERT(builtEmpty.find("console=ttyAMA0") != std::string::npos, "Should add console argument if base is empty");
    TEST_ASSERT(builtEmpty.find("earlycon=pl011,0x09000000") != std::string::npos, "Should add earlycon argument if base is empty");
}

void testImageConverter() {
    // 1. Test parsing range sets
    auto ranges = TransferListParser::parseRangeSet("4,0,10,20,30");
    TEST_ASSERT(ranges.size() == 2, "Should parse 2 ranges");
    TEST_ASSERT(ranges[0].first == 0 && ranges[0].second == 10, "Range 0 bounds check");
    TEST_ASSERT(ranges[1].first == 20 && ranges[1].second == 30, "Range 1 bounds check");

    // 2. Test mock transfer list parsing & image construction
    std::filesystem::path testDir = "test_image_converter_dir";
    std::filesystem::create_directory(testDir);

    std::filesystem::path transferPath = testDir / "system.transfer.list";
    {
        std::ofstream out(transferPath);
        out << "4\n3\n0\n0\n";
        out << "new 4,0,1,2,3\n";
        out << "zero 2,1,2\n";
    }

    std::filesystem::path datPath = testDir / "system.new.dat";
    {
        std::ofstream out(datPath, std::ios::binary);
        std::vector<char> blockA(4096, 'A');
        std::vector<char> blockB(4096, 'B');
        out.write(blockA.data(), blockA.size());
        out.write(blockB.data(), blockB.size());
    }

    std::filesystem::path imgPath = testDir / "system.img";
    std::string err;
    bool ok = DatImageBuilder::build(datPath, transferPath, imgPath, nullptr, &err);
    TEST_ASSERT(ok, "DatImageBuilder::build should succeed");
    
    // Check built file size
    TEST_ASSERT(std::filesystem::exists(imgPath), "Reconstructed image must exist");
    TEST_ASSERT(std::filesystem::file_size(imgPath) == 3 * 4096, "Reconstructed image size must match block count");

    // Read back blocks and verify
    std::ifstream in(imgPath, std::ios::binary);
    std::vector<char> block(4096);
    
    // Block 0: should be all 'A'
    in.read(block.data(), 4096);
    bool allA = true;
    for (char c : block) { if (c != 'A') allA = false; }
    TEST_ASSERT(allA, "Block 0 must be all A");

    // Block 1: should be all 0 (zero command)
    in.read(block.data(), 4096);
    bool allZero = true;
    for (char c : block) { if (c != 0) allZero = false; }
    TEST_ASSERT(allZero, "Block 1 must be zeroed");

    // Block 2: should be all 'B'
    in.read(block.data(), 4096);
    bool allB = true;
    for (char c : block) { if (c != 'B') allB = false; }
    TEST_ASSERT(allB, "Block 2 must be all B");

    in.close();

    // 3. Test verification (magic at offset 1080)
    {
        std::fstream out(imgPath, std::ios::in | std::ios::out | std::ios::binary);
        out.seekp(1080);
        char magic[2] = {0x53, (char)0xEF};
        out.write(magic, 2);
    }
    auto vr = DatImageBuilder::verify(imgPath);
    TEST_ASSERT(vr.isValidExt4, "Ext4 magic validation should detect mock magic");
    TEST_ASSERT(!vr.sha256.empty(), "SHA-256 digest should be generated");

    // Pre-compressed brotli payload for "Hello Brotli!"
    std::vector<uint8_t> compressedPayload = {
        0x0F, 0x06, 0x80, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20,
        0x42, 0x72, 0x6F, 0x74, 0x6C, 0x69, 0x21, 0x03
    };
    std::filesystem::path brPath = testDir / "hello.br";
    std::filesystem::path decompressedPath = testDir / "hello.txt";
    {
        std::ofstream out(brPath, std::ios::binary);
        out.write(reinterpret_cast<const char*>(compressedPayload.data()), compressedPayload.size());
    }
    ok = BrotliDecompressor::decompress(brPath, decompressedPath, nullptr, &err);
    TEST_ASSERT(ok, "BrotliDecompressor::decompress should succeed");
    TEST_ASSERT(std::filesystem::exists(decompressedPath), "Decompressed file must exist");
    
    std::ifstream txtIn(decompressedPath);
    std::string decompressedText;
    std::getline(txtIn, decompressedText);
    TEST_ASSERT(decompressedText == "Hello Brotli!", "Decompressed text must match original");
    txtIn.close();

    // Cleanup
    std::filesystem::remove_all(testDir);
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

    manager.getSession().setQemuCommandLine("qemu-system-aarch64 -cpu cortex-a53 -append 'modified'");
    manager.getSession().setOriginalQemuCommandLine("qemu-system-aarch64 -cpu cortex-a53 -append 'original'");
    TEST_ASSERT(manager.getSession().getQemuCommandLine() == "qemu-system-aarch64 -cpu cortex-a53 -append 'modified'", "Qemu cmdline must match");
    TEST_ASSERT(manager.getSession().getOriginalQemuCommandLine() == "qemu-system-aarch64 -cpu cortex-a53 -append 'original'", "Original Qemu cmdline must match");
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

void testKernelAnalysis() {
    std::filesystem::path mapPath = "test_System.map";
    std::ofstream out(mapPath);
    out << "ffffff8008080000 T _text\n";
    out << "ffffff800888a250 t qcom_scm_set_warm_boot_addr\n";
    out << "ffffff800888a300 t other_func\n";
    out.close();

    kernel_analysis::SymbolResolver resolver;
    bool success = resolver.loadSystemMapSymbols(mapPath.string());
    std::filesystem::remove(mapPath);

    TEST_ASSERT(success, "System.map parsing should succeed");
    TEST_ASSERT(resolver.getSymbolCount() == 3, "Symbol count must equal 3");

    // Exact matches
    TEST_ASSERT(resolver.resolveAddress(0xffffff8008080000) == "_text+0x0", "Should resolve exact _text start");
    TEST_ASSERT(resolver.resolveAddress(0xffffff800888a250) == "qcom_scm_set_warm_boot_addr+0x0", "Should resolve exact function start");

    // Offsets
    TEST_ASSERT(resolver.resolveAddress(0xffffff8008080010) == "_text+0x10", "Should resolve offset from _text");
    TEST_ASSERT(resolver.resolveAddress(0xffffff800888a25c) == "qcom_scm_set_warm_boot_addr+0xc", "Should resolve offset inside SCM warm boot call");

    // Address before first symbol
    TEST_ASSERT(resolver.resolveAddress(0xffffff8008000000) == "0xffffff8008000000", "Should fall back to raw hex if before first symbol");
}

#include "behavior_exploration_tool/bet_system.h"
#include <filesystem>

void testBehaviorExplorationTool() {
    std::cout << "\n--- Behavior Exploration Tool (BET) Tests ---\n";

    std::string testDbDir = "test_bet_db";
    std::string testDbFile = testDbDir + "/bet_experiments.json";
    
    // Clean potential leftover
    std::filesystem::remove_all(testDbDir);

    // 1. Initialize BetSystem
    bet::BetSystem& system = bet::BetSystem::instance();
    system.initialize(testDbDir);
    
    // 2. Start a session
    system.startSession("test_rom", "5.4.0", "1.0.0");
    int activeId = system.getActiveBootId();
    TEST_ASSERT(activeId == 1, "First session ID must be 1");

    // 3. Register intercepts
    std::vector<uint64_t> before = { 0xC2000601, 1, 0x82000111, 0, 0, 0, 0, 0 };
    std::vector<uint64_t> after = { 0, 1, 0, 0, 0, 0, 0, 0 };
    std::map<std::string, std::string> meta = { {"symbol", "qcom_scm_set_warm_boot_addr+0xc"} };
    
    // Feed 6 identical SMC intercepts at the same PC to trigger BlockerDetector's Repeated SMC rule
    for (int i = 0; i < 6; ++i) {
        system.registerIntercept("SMC", "C2000601", 0xffffff800888a25c, 0xffffff800888a200, 0xffffffc000100000, before, before, after, true, "", meta);
    }

    // Push log lines (including driver probe success to test ProgressAnalyzer)
    system.pushLogLine("[0.012345] Booting Linux...");
    system.pushLogLine("[1.020304] pm8005-regulator: probe of pm8005-regulator succeeded");
    system.pushLogLine("[1.500000] Kernel panic - not syncing: Attempted to kill init!");

    // 4. End the session
    system.endSession("Failed (Panic)");

    // 5. Verify database saved and contains the run
    TEST_ASSERT(std::filesystem::exists(testDbFile), "Database file must be written to disk");
    
    bet::ExperimentRun run;
    bool found = system.getDatabase().getRun(1, run);
    TEST_ASSERT(found, "Run #1 must exist in the database");
    TEST_ASSERT(run.romName == "test_rom", "ROM name must match 'test_rom'");
    TEST_ASSERT(run.progress.totalSmcCount == 6, "Total SMC count must equal 6");
    TEST_ASSERT(run.progress.uniqueSmcCount == 1, "Unique SMC count must equal 1");
    TEST_ASSERT(run.progress.initializedDriversCount == 1, "Initialized drivers count must equal 1");
    TEST_ASSERT(run.progress.initializedDrivers[0] == "pm8005-regulator", "Driver name must be parsed correctly");

    // 6. Verify timeline parsing
    TEST_ASSERT(run.timeline.size() >= 4, "Timeline must contain BootStart, Kernel Entry, Driver success, and Panic");
    bool hasDriverEvent = false;
    bool hasPanicEvent = false;
    for (const auto& te : run.timeline) {
        if (te.category == "Driver") hasDriverEvent = true;
        if (te.category == "Panic") hasPanicEvent = true;
    }
    TEST_ASSERT(hasDriverEvent, "Timeline must capture the driver probe event");
    TEST_ASSERT(hasPanicEvent, "Timeline must capture the kernel panic event");

    // 7. Verify comparison engine works (create session 2 with more progress)
    system.startSession("test_rom", "5.4.0", "1.0.0");
    TEST_ASSERT(system.getActiveBootId() == 2, "Second session ID must be 2");
    
    // Run 2 has 1 extra driver probe success
    system.registerIntercept("SMC", "C2000601", 0xffffff800888a25c, 0xffffff800888a200, 0xffffffc000100000, before, before, after, true, "", meta);
    system.pushLogLine("[1.020304] pm8005-regulator: probe of pm8005-regulator succeeded");
    system.pushLogLine("[2.010101] ufs_qcom: probe of ufs_qcom succeeded");
    system.endSession("Success");

    bet::ExperimentRun run2;
    found = system.getDatabase().getRun(2, run2);
    TEST_ASSERT(found, "Run #2 must exist in the database");
    
    // Compare Run 2 with Run 1
    bet::ComparisonEngine compEngine;
    bet::ComparisonReport comp = compEngine.compareRuns(run2.progress, run.progress);
    TEST_ASSERT(comp.progressStatus == "Progressed", "Run #2 status must be 'Progressed' compared to Run 1");
    TEST_ASSERT(comp.driverCountDelta == 1, "Driver count delta must equal +1");
    TEST_ASSERT(comp.newDrivers.size() == 1, "Should detect 1 new driver");
    TEST_ASSERT(comp.newDrivers[0] == "ufs_qcom", "New driver must be 'ufs_qcom'");

    // Clean up
    std::filesystem::remove_all(testDbDir);
    std::cout << "[PASS] Behavior Exploration Tool tests completed successfully!\n";
}

#include "behavior_exploration_tool/override_engine.h"
#include "behavior_exploration_tool/bet_replayer.h"

void testOverrideAndReplay() {
    std::cout << "\n--- Override Engine & Replay Client Tests ---\n";

    std::string testOverridesDir = "test_overrides_dir";
    std::filesystem::create_directories(testOverridesDir);
    std::string testOverridesFile = testOverridesDir + "/bet_overrides.json";

    // --- 1. Test Override Rule Schema Validation ---
    {
        bet::OverrideEngine engine;
        
        // Write invalid JSON rule file (invalid scope)
        std::ofstream out(testOverridesFile);
        out << R"({
            "enabled": true,
            "rules": [
                {
                    "id": "rule_invalid",
                    "enabled": true,
                    "scope": "invalid-scope",
                    "category": "SMC",
                    "name": "0xC2000601",
                    "action": "override_registers",
                    "overrides": { "x0": 0 }
                }
            ]
        })";
        out.close();
        
        bool ok = engine.loadFromFile(testOverridesFile);
        TEST_ASSERT(!ok, "OverrideEngine must reject rules with invalid scopes");
    }

    {
        bet::OverrideEngine engine;
        // Write invalid JSON rule file (invalid action name)
        std::ofstream out(testOverridesFile);
        out << R"({
            "enabled": true,
            "rules": [
                {
                    "id": "rule_invalid_action",
                    "enabled": true,
                    "scope": "global",
                    "category": "SMC",
                    "name": "0xC2000601",
                    "action": "invalid_action",
                    "overrides": { "x0": 0 }
                }
            ]
        })";
        out.close();
        
        bool ok = engine.loadFromFile(testOverridesFile);
        TEST_ASSERT(!ok, "OverrideEngine must reject rules with unsupported actions");
    }

    {
        bet::OverrideEngine engine;
        // Write valid JSON rules with scoped priorities
        std::ofstream out(testOverridesFile);
        out << R"({
            "enabled": true,
            "rules": [
                {
                    "id": "global_rule",
                    "enabled": true,
                    "scope": "global",
                    "description": "Global SMC override description",
                    "category": "SMC",
                    "name": "0xC2000601",
                    "action": "override_registers",
                    "overrides": { "x0": 100 }
                },
                {
                    "id": "rom_rule",
                    "enabled": true,
                    "scope": "rom",
                    "romName": "test_rom_specific",
                    "description": "ROM specific SMC override description",
                    "category": "SMC",
                    "name": "0xC2000601",
                    "action": "override_registers",
                    "overrides": { "x0": 200 }
                },
                {
                    "id": "boot_rule",
                    "enabled": true,
                    "scope": "boot-id",
                    "bootId": 5,
                    "description": "Boot specific SMC override description",
                    "category": "SMC",
                    "name": "0xC2000601",
                    "action": "override_registers",
                    "overrides": { "x0": 300 }
                }
            ]
        })";
        out.close();

        bool ok = engine.loadFromFile(testOverridesFile);
        TEST_ASSERT(ok, "OverrideEngine must successfully load valid configuration rules");

        // --- 2. Test Rule Scoped Priority Matching ---
        std::string ruleId, ruleDesc;
        std::map<std::string, uint64_t> overrides;
        
        // global match
        bool found = engine.findOverride("SMC", "0xC2000601", "other_rom", "5.4", 1, ruleId, ruleDesc, overrides);
        TEST_ASSERT(found && ruleId == "global_rule" && overrides["x0"] == 100, "Should match global scope with lowest priority");

        // rom matches and overrides global
        found = engine.findOverride("SMC", "0xC2000601", "test_rom_specific", "5.4", 1, ruleId, ruleDesc, overrides);
        TEST_ASSERT(found && ruleId == "rom_rule" && overrides["x0"] == 200, "Should match rom scope override over global");

        // boot-id matches and overrides both
        found = engine.findOverride("SMC", "0xC2000601", "test_rom_specific", "5.4", 5, ruleId, ruleDesc, overrides);
        TEST_ASSERT(found && ruleId == "boot_rule" && overrides["x0"] == 300, "Should match boot-id scope override with highest priority");
    }

    // --- 3. Test Offline Replayer and filtering ---
    std::string testTraceFile = testOverridesDir + "/test_trace.json";
    {
        // Construct mock trace array
        nlohmann::json jTrace = nlohmann::json::array();
        
        // Add boot start
        jTrace.push_back({
            {"bootId", 1}, {"elapsedSeconds", 0.0}, {"timestamp", "2026-07-17T00:00:00Z"},
            {"type", "BootStart"}, {"category", "System"}, {"name", "boot_start"},
            {"cpu", -1}, {"pc", "0x0"}, {"lr", "0x0"}, {"sp", "0x0"},
            {"registersBefore", nlohmann::json::array()},
            {"registersDispatcher", nlohmann::json::array()},
            {"registersAfter", nlohmann::json::array()},
            {"handled", false}, {"overrideRuleId", ""}, {"details", "boot start"}, {"metadata", nlohmann::json::object()}
        });

        // Add intercept 1 (Category SMC, PC=0x1000)
        jTrace.push_back({
            {"bootId", 1}, {"elapsedSeconds", 0.5}, {"timestamp", "2026-07-17T00:00:00Z"},
            {"type", "GuestIntercept"}, {"category", "SMC"}, {"name", "0xC2000601"},
            {"cpu", 0}, {"pc", "0x1000"}, {"lr", "0x0"}, {"sp", "0x0"},
            {"registersBefore", {1, 2}},
            {"registersDispatcher", {1, 2}},
            {"registersAfter", {0, 1}},
            {"handled", true}, {"overrideRuleId", "global_rule"}, {"details", "SMC 0xC2000601 matched"}, {"metadata", nlohmann::json::object()}
        });

        // Add intercept 2 (Category MMIO, PC=0x2000)
        jTrace.push_back({
            {"bootId", 1}, {"elapsedSeconds", 1.0}, {"timestamp", "2026-07-17T00:00:00Z"},
            {"type", "GuestIntercept"}, {"category", "MMIO"}, {"name", "0x01C20000"},
            {"cpu", 1}, {"pc", "0x2000"}, {"lr", "0x0"}, {"sp", "0x0"},
            {"registersBefore", {10}},
            {"registersDispatcher", {10}},
            {"registersAfter", {20}},
            {"handled", true}, {"overrideRuleId", ""}, {"details", "MMIO read"}, {"metadata", nlohmann::json::object()}
        });

        std::ofstream out(testTraceFile);
        out << jTrace.dump(4);
        out.close();
    }

    {
        bet::BetReplayer replayer;
        bool ok = replayer.loadRunTrace(testTraceFile);
        TEST_ASSERT(ok, "BetReplayer must successfully load trace JSON");
        TEST_ASSERT(replayer.getEventCount() == 3, "Trace must have exactly 3 events");

        // Test navigation
        bet::BetEvent ev;
        bool step = replayer.jumpToEvent(1, ev);
        TEST_ASSERT(step && ev.category == "SMC" && ev.pc == 0x1000, "Should jump to event index 1");

        step = replayer.nextEvent(ev);
        TEST_ASSERT(step && ev.category == "MMIO" && ev.pc == 0x2000, "Should step to next event");

        step = replayer.prevEvent(ev);
        TEST_ASSERT(step && ev.category == "SMC" && ev.pc == 0x1000, "Should step to previous event");

        // Test Filtering (Category filter)
        replayer.setCategoryFilter("MMIO");
        TEST_ASSERT(replayer.getEventCount() == 1, "Filtered count for Category 'MMIO' must equal 1");
        replayer.jumpToEvent(0, ev);
        TEST_ASSERT(ev.pc == 0x2000, "Filtered event must be MMIO at PC 0x2000");

        // Test Filtering (PC filter)
        replayer.clearFilters();
        replayer.setPcFilter(0x1000);
        TEST_ASSERT(replayer.getEventCount() == 1, "Filtered count for PC 0x1000 must equal 1");
        replayer.jumpToEvent(0, ev);
        TEST_ASSERT(ev.category == "SMC", "Filtered event must be SMC at PC 0x1000");

        // Test Search query keyword
        replayer.clearFilters();
        auto searchRes = replayer.searchEvents("Matched");
        TEST_ASSERT(searchRes.size() == 1 && searchRes[0].category == "SMC", "Search query 'Matched' must return 1 matching SMC event");

        // Test Export
        std::string jsonStr = replayer.exportEventJson(1);
        TEST_ASSERT(jsonStr.find("0xC2000601") != std::string::npos, "Exported event JSON must contain the SMC name '0xC2000601'");
    }

    std::filesystem::remove_all(testOverridesDir);
    std::cout << "[PASS] Override Engine & Replay Client tests completed successfully!\n";
}

void runRealBootForDiagnostic(int durationSeconds) {
    std::cout << "\n--- Headless VM Boot Tracing ---\n";
    int argc = 1;
    char* argv[] = { (char*)"run_tests.exe", nullptr };
    QCoreApplication app(argc, argv);

    core::ConfigManager::initializeDefaultConfigs();

    VmManager vmManager;
    std::string romName = "PixelExperience_violet-13.0-20240127-0711-OFFICIAL";
    std::filesystem::path romDir = "C:/Users/ABISIN RAJ/Desktop/Projects/CustomAndroidEmulator/build/launcher/roms/PixelExperience_violet-13.0-20240127-0711-OFFICIAL";
    
    std::cout << "Starting VM for ROM: " << romName << " at " << romDir.string() << std::endl;
    bool started = vmManager.startVm(romName, romDir);
    if (!started) {
        std::cout << "Failed to start VM!" << std::endl;
        return;
    }

    // Set a timer to stop VM and exit app
    QTimer::singleShot(durationSeconds * 1000, [&]() {
        std::cout << "Timer expired. Stopping VM..." << std::endl;
        vmManager.stopVm();
        // Wait 1 second for QEMU process to stop cleanly and trigger state change
        QTimer::singleShot(1000, [&]() {
            app.quit();
        });
    });

    app.exec();
    std::cout << "Headless VM Boot Tracing completed.\n";
}

int main(int argc, char* argv[]) {
    core::initLogger();

    // Check for real VM command-line flag
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--run-real-vm") {
            runRealBootForDiagnostic(20);
            return 0;
        }
    }

    std::cout << "======================================\n";
    std::cout << "Starting CustomAndroidEmulator Tests\n";
    std::cout << "======================================\n";

    testBehaviorExplorationTool();
    testOverrideAndReplay();
    testBootImgParser();
    testVbmetaParser();
    testExt4Parser();
    testBootManagerValidation();
    testConfigManager();
    testBootAssetLoader();
    testVmProfile();
    testQemuArguments();
    testKernelCommandLine();
    testImageConverter();
    testEventDispatcher();
    testPanicDetector();
    testBootPipelineManager();
    testKernelAnalysis();

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

    // ===================================================================
    // Section: SmcDispatcher — Firmware Compatibility Layer
    // ===================================================================
    std::cout << "\n--- SmcDispatcher Tests ---\n";

    // --- Test 1: SmcId decodes 0xC2000601 correctly ---
    {
        firmware::SmcId id;
        id.raw = 0xC2000601U;
        TEST_ASSERT(id.isFastCall() == true,  "0xC2000601 must be a Fast Call");
        TEST_ASSERT(id.is64bit()    == true,  "0xC2000601 must be SMC64");
        TEST_ASSERT(id.owner()      == 0x02,  "0xC2000601 owner must be 0x02 (SIP)");
        TEST_ASSERT(id.service()    == 0x06,  "0xC2000601 service must be 0x06 (Info)");
        TEST_ASSERT(id.command()    == 0x01,  "0xC2000601 command must be 0x01 (INFO_IS_CALL_AVAIL)");
    }

    // --- Test 2: Dispatcher without services returns NOT_SUPPORTED ---
    {
        firmware::SmcDispatcher dispatcher;
        firmware::SmcResult result = dispatcher.dispatch(0xC2000601ULL);
        TEST_ASSERT(!result.handled,
            "Empty dispatcher must return handled=false for any call");
        TEST_ASSERT(result.x0 == firmware::SMCCC_RET_NOT_SUPPORTED,
            "Empty dispatcher must return SMCCC_RET_NOT_SUPPORTED");
    }

    // --- Test 3: QcomBootService registered — serviceCount = 1 ---
    {
        firmware::SmcDispatcher dispatcher;
        dispatcher.registerService(std::make_unique<firmware::QcomBootService>());
        TEST_ASSERT(dispatcher.serviceCount() == 1,
            "Dispatcher must have exactly 1 registered service after registration");
    }

    // --- Test 4: QCOM_SCM_BOOT_SET_ADDR_MC dispatched and returns SUCCESS ---
    {
        firmware::SmcDispatcher dispatcher;
        dispatcher.registerService(std::make_unique<firmware::QcomBootService>());

        constexpr uint64_t warmBootAddr = 0xFFFFFF8008090000ULL;
        firmware::SmcResult result = dispatcher.dispatch(
            0xC2000111ULL,   // x0 = SMC ID (Service=1, Cmd=0x11)
            1ULL,            // x1 = arginfo
            warmBootAddr,    // x2 = physical entry point
            ~0ULL,           // x3
            ~0ULL,           // x4
            ~0ULL,           // x5
            0ULL             // x6 = flags
        );

        TEST_ASSERT(result.handled,
            "QCOM_SCM_BOOT_SET_ADDR_MC must be handled by QcomBootService");
        TEST_ASSERT(result.x0 == firmware::SMCCC_RET_SUCCESS,
            "QCOM_SCM_BOOT_SET_ADDR_MC must return X0 = 0 (SMCCC_RET_SUCCESS)");
    }

    // --- Test 5: QcomInfoService registered and returns CALL_AVAIL ---
    {
        firmware::SmcDispatcher dispatcher;
        dispatcher.registerService(std::make_unique<firmware::QcomInfoService>());

        firmware::SmcResult result = dispatcher.dispatch(
            0xC2000601ULL,   // x0 = SMC ID (Service=6, Cmd=1)
            1ULL,            // x1 = arginfo
            0x82000111ULL    // x2 = Function ID queried
        );

        TEST_ASSERT(result.handled,
            "QCOM_SCM_INFO_IS_CALL_AVAIL must be handled by QcomInfoService");
        TEST_ASSERT(result.x0 == firmware::SMCCC_RET_SUCCESS,
            "QCOM_SCM_INFO_IS_CALL_AVAIL must return X0 = 0 (SUCCESS)");
        TEST_ASSERT(result.x1 == 1,
            "QCOM_SCM_INFO_IS_CALL_AVAIL must return X1 = 1 (Available)");
    }

    // --- Test 6: Unknown command in boot service returns NOT_SUPPORTED ---
    {
        firmware::SmcDispatcher dispatcher;
        dispatcher.registerService(std::make_unique<firmware::QcomBootService>());

        // 0xC2000199 — valid owner+service (1) but unknown command 0x99
        firmware::SmcResult result = dispatcher.dispatch(0xC2000199ULL);
        TEST_ASSERT(!result.handled,
            "Unknown boot service command must return handled=false");
        TEST_ASSERT(result.x0 == firmware::SMCCC_RET_NOT_SUPPORTED,
            "Unknown boot service command must return SMCCC_RET_NOT_SUPPORTED");
    }

    // --- Test 7: Completely unknown SMC ID returns NOT_SUPPORTED ---
    {
        firmware::SmcDispatcher dispatcher;
        dispatcher.registerService(std::make_unique<firmware::QcomBootService>());

        // 0x84000001 — ARM SMCCC Version call (unrelated owner)
        firmware::SmcResult result = dispatcher.dispatch(0x84000001ULL);
        TEST_ASSERT(!result.handled,
            "Unknown owner SMC call must return handled=false");
        TEST_ASSERT(result.x0 == firmware::SMCCC_RET_NOT_SUPPORTED,
            "Unknown owner SMC call must return SMCCC_RET_NOT_SUPPORTED");
    }

    // --- Test 8: SmcId::toString() produces a non-empty readable string ---
    {
        firmware::SmcId id;
        id.raw = 0xC2000601U;
        std::string s = id.toString();
        TEST_ASSERT(!s.empty(),
            "SmcId::toString() must return a non-empty string");
        TEST_ASSERT(s.find("C2000601") != std::string::npos,
            "SmcId::toString() must contain the raw hex value");
    }

    std::cout << "======================================\n";
    std::cout << "All unit tests completed successfully!\n";
    std::cout << "======================================\n";
    return 0;
}
