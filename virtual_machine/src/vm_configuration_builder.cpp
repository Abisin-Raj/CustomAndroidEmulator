#include "virtual_machine/vm_configuration_builder.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

QemuConfigParameters VmConfigurationBuilder::build(
    const std::filesystem::path& romDir, 
    const VmProfile& profile,
    const std::string& tempKernelPath,
    const std::string& tempRamdiskPath
) {
    QemuConfigParameters params;
    params.cpu       = "cortex-a55";
    params.machine   = "virt,gic-version=3";
    params.smp       = "1";
    params.memory    = "4096";
    params.nographic = false;
    params.enableVnc = true;
    params.displayDevice = "ramfb";
    params.vncAddress = "127.0.0.1:0";
    params.enableMmioTrace = false;
    params.biosPath  = "C:/Users/ABISIN RAJ/Desktop/Projects/CustomAndroidQEMU/pc-bios";

    // Use rebuilt kernel if present, otherwise tempKernelPath
    if (std::filesystem::exists("C:/qemu_work/kernel.img")) {
        params.kernelPath = "C:/qemu_work/kernel.img";
    } else {
        params.kernelPath = tempKernelPath;
    }

    // Use boot_ramdisk.gz (normal Android system boot)
    if (std::filesystem::exists("C:/Users/ABISIN RAJ/Desktop/Projects/CustomAndroidEmulator/boot_ramdisk.gz")) {
        params.ramdiskPath = "C:/Users/ABISIN RAJ/Desktop/Projects/CustomAndroidEmulator/boot_ramdisk.gz";
    } else if (std::filesystem::exists("C:/Users/ABISIN RAJ/Desktop/Projects/CustomAndroidEmulator/recovery_ramdisk.gz")) {
        params.ramdiskPath = "C:/Users/ABISIN RAJ/Desktop/Projects/CustomAndroidEmulator/recovery_ramdisk.gz";
    } else {
        params.ramdiskPath = tempRamdiskPath;
    }

    // Prioritize virt_spmi_compact_ion DTB with ION & hardware mappings
    if (std::filesystem::exists("C:/qemu_work/virt_spmi_compact_ion.dtb")) {
        params.dtbPath = "C:/qemu_work/virt_spmi_compact_ion.dtb";
    } else if (std::filesystem::exists("C:/qemu_work/virt_spmi_compact.dtb")) {
        params.dtbPath = "C:/qemu_work/virt_spmi_compact.dtb";
    }

    // Attach eMMC GPT overlay disk and instant checkpoint resume
    if (std::filesystem::exists("C:/qemu_work/emmc_overlay.qcow2")) {
        params.diskImagePath = "C:/qemu_work/emmc_overlay.qcow2";
        params.loadVmSnapshot = "cp02_rtr_token";
    } else {
        std::filesystem::path systemImg = romDir / "system.img";
        std::filesystem::path vendorImg = romDir / "vendor.img";
        if (std::filesystem::exists(systemImg)) {
            params.systemImagePath = systemImg.string();
        }
        if (std::filesystem::exists(vendorImg)) {
            params.vendorImagePath = vendorImg.string();
        }
    }

    // Correct Android 13 SDHCI boot command line
    params.cmdline = "init=/init nokaslr norandmaps androidboot.bootdevice=7c4000.sdhci console=ttyAMA0,115200 printk.devkmsg=on androidboot.debug_level=0x4f4c androidboot.selinux=permissive enforcing=0 androidboot.crypto_state=unencrypted androidboot.hardware=qcom androidboot.hardware.platform=sm6150 ro.crypto.state=unencrypted";

    return params;
}
