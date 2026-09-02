@echo off
title Custom Android Emulator - Redmi Note 7 Pro (QEMU Native Display)
cd /d "C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator"

echo ================================================================================
echo Launching Real Qualcomm Snapdragon 675 / Redmi Note 7 Pro Android Emulator
echo ================================================================================
echo.

"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidQEMU\build\qemu-system-aarch64.exe" ^
    -M virt,gic-version=3 ^
    -cpu cortex-a55 ^
    -smp 4 ^
    -m 4096 ^
    -device ramfb ^
    -display sdl ^
    -device virtio-tablet-pci ^
    -device virtio-keyboard-pci ^
    -kernel "C:\qemu_work\kernel.img" ^
    -initrd "C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\boot_ramdisk.gz" ^
    -dtb "C:\qemu_work\virt_spmi_compact.dtb" ^
    -append "rdinit=/init console=ttyMSM0,115200n8 nokaslr androidboot.hardware=qcom androidboot.bootdevice=1d84000.ufshc androidboot.mode=normal androidboot.bootmode=normal androidboot.force_normal_boot=1 androidboot.selinux=permissive enforcing=0 earlycon=msm_geni_serial,0x880000 loglevel=8 ignore_loglevel panic=0" ^
    -serial stdio

pause
