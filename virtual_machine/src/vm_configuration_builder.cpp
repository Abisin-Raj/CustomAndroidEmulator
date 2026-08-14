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
    params.memory    = "4096";
    params.nographic = false;
    params.enableVnc = true;
    params.displayDevice = "ramfb";
    params.vncAddress = "127.0.0.1:0";
    params.enableMmioTrace = false;

    // Use rebuilt kernel if present, otherwise tempKernelPath
    if (std::filesystem::exists("C:/qemu_work/kernel.img")) {
        params.kernelPath = "C:/qemu_work/kernel.img";
    } else {
        params.kernelPath = tempKernelPath;
    }

    // Phase 11.1: Use boot_ramdisk.gz (normal Android system boot)
    if (std::filesystem::exists("C:/Users/ABISIN RAJ/Desktop/Projects/CustomAndroidEmulator/boot_ramdisk.gz")) {
        params.ramdiskPath = "C:/Users/ABISIN RAJ/Desktop/Projects/CustomAndroidEmulator/boot_ramdisk.gz";
    } else if (std::filesystem::exists("C:/Users/ABISIN RAJ/Desktop/Projects/CustomAndroidEmulator/recovery_ramdisk.gz")) {
        params.ramdiskPath = "C:/Users/ABISIN RAJ/Desktop/Projects/CustomAndroidEmulator/recovery_ramdisk.gz";
    } else {
        params.ramdiskPath = tempRamdiskPath;
    }

    // Use compact SPMI DTB
    if (std::filesystem::exists("C:/qemu_work/virt_spmi_compact.dtb")) {
        params.dtbPath = "C:/qemu_work/virt_spmi_compact.dtb";
    }

    // Set verified guest command line for Android userspace boot
    params.cmdline = "console=ttyMSM0,115200n8 nokaslr androidboot.selinux=permissive enforcing=0 androidboot.hardware=qcom androidboot.mode=normal androidboot.bootdevice=1d84000.ufshc earlycon=msm_geni_serial,0x880000 loglevel=8 ignore_loglevel";

    return params;
}
