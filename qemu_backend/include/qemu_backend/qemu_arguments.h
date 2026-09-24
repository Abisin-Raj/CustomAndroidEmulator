#pragma once
#include <string>
#include <vector>
#include <QStringList>

struct QemuConfigParameters {
    std::string cpu = "cortex-a53";
    std::string machine = "virt";
    std::string memory = "1024M";
    std::string kernelPath;
    std::string ramdiskPath;
    std::string dtbPath;
    std::string cmdline;
    std::string extraArgs;
    bool nographic      = false;
    bool enableVnc      = true;
    std::string vncAddress = "127.0.0.1:0";
    std::string displayDevice = "ramfb";
    // When true, appends -d unimp,guest_errors -D qemu_trace.log to observe
    // guest accesses to unmapped/unimplemented MMIO regions.
    bool enableMmioTrace = false;
    // When true, appends -dumpdtb qemu_generated.dtb so QEMU writes its
    // auto-generated DTB to disk for inspection and CPU reg comparison.
    bool enableDtbDump   = false;
    // When true, adds -netdev user,id=net0,hostfwd=tcp::<hostPort>-:7777 -device virtio-net-pci,netdev=net0
    // so the Qt host can TCP-connect to input_bridge inside the guest on port 7777.
    // Port 7777 is used instead of 5555 (which is claimed by the Android ADB server on the host).
    bool enableInputBridge  = true;
    int  inputBridgeHostPort = 7777;
    // Block drives for Android partitions (Phase 11.2)
    std::string systemImagePath;
    std::string vendorImagePath;
    // eMMC disk image overlay & BIOS
    std::string biosPath = "C:/Users/ABISIN RAJ/Desktop/Projects/CustomAndroidQEMU/pc-bios";
    std::string smp = "1";
    std::string diskImagePath;
    std::string loadVmSnapshot;
};

class QemuArguments {
public:
    /**
     * @brief Generates the command-line argument list for QEMU matching the configuration parameters.
     */
    static QStringList build(const QemuConfigParameters& params);
};
