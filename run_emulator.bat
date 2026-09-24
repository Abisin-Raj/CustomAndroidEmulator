@echo off
title Custom Android Emulator - Redmi Note 7 Pro (Snapdragon 675 / Android 13)
setlocal enabledelayedexpansion

cd /d "C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator"

echo ================================================================================
echo  Qualcomm Snapdragon 675 / Redmi Note 7 Pro Android Emulator (QEMU Native SDL)
echo ================================================================================
echo.

:: -----------------------------------------------------------------------------
:: Path Configuration
:: -----------------------------------------------------------------------------
set "QEMU_EXE=C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidQEMU\build\qemu-system-aarch64.exe"
set "PC_BIOS=C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidQEMU\pc-bios"
set "KERNEL_IMG=C:\qemu_work\kernel.img"
set "INITRD_IMG=C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\boot_ramdisk.gz"
set "DTB_IMG=C:\qemu_work\virt_spmi_compact_ion.dtb"
set "DISK_IMG=C:\qemu_work\emmc_overlay.qcow2"
set "CONSOLE_LOG=C:\qemu_work\sf_console.log"

:: -----------------------------------------------------------------------------
:: Pre-Flight Path Verification
:: -----------------------------------------------------------------------------
if not exist "%QEMU_EXE%" (
    echo [ERROR] QEMU executable not found at:
    echo         "%QEMU_EXE%"
    goto :error
)
if not exist "%PC_BIOS%" (
    echo [ERROR] pc-bios directory not found at:
    echo         "%PC_BIOS%"
    goto :error
)
if not exist "%KERNEL_IMG%" (
    echo [ERROR] Kernel image not found at:
    echo         "%KERNEL_IMG%"
    goto :error
)
if not exist "%INITRD_IMG%" (
    echo [ERROR] Boot ramdisk not found at:
    echo         "%INITRD_IMG%"
    goto :error
)
if not exist "%DTB_IMG%" (
    echo [ERROR] DTB file not found at:
    echo         "%DTB_IMG%"
    goto :error
)
if not exist "%DISK_IMG%" (
    echo [ERROR] eMMC disk overlay not found at:
    echo         "%DISK_IMG%"
    goto :error
)

:: -----------------------------------------------------------------------------
:: Process Cleanup: Avoid Port / File Locking Conflicts
:: -----------------------------------------------------------------------------
tasklist /fi "imagename eq qemu-system-aarch64.exe" 2>nul | find /i "qemu-system-aarch64.exe" >nul
if not errorlevel 1 (
    echo [!] Previous QEMU instance detected.
    echo [*] Terminating prior instance to free ports and eMMC locks...
    taskkill /f /im qemu-system-aarch64.exe >nul 2>&1
    ping -n 2 127.0.0.1 >nul 2>&1
)

:: -----------------------------------------------------------------------------
:: Boot Mode Selection
:: -----------------------------------------------------------------------------
set "BOOT_MODE=%~1"
if not "%BOOT_MODE%"=="" goto :parse_mode

:prompt_mode
echo   Select Boot Mode:
echo     [1] Fast Boot  - Resume CP-02 (SurfaceFlinger + BootAnimation ready, ~2s) [DEFAULT]
echo     [2] Fast Boot  - Resume CP-01 (SurfaceFlinger stable, ~2s)
echo     [3] Cold Boot  - Full Android 13 boot from scratch (~2-3 min)
echo     [4] Debug Boot - Cold Boot with GDB 1776 and QMP 4444 stubs
echo.
choice /c 1234 /t 5 /d 1 /m "  Choose mode [auto-selects 1 in 5s]"
set "BOOT_MODE=%errorlevel%"

:parse_mode
set "EXTRA_ARGS="
if "%BOOT_MODE%"=="1" goto :mode_cp02
if "%BOOT_MODE%"=="--fast" goto :mode_cp02
if "%BOOT_MODE%"=="-fast" goto :mode_cp02
if "%BOOT_MODE%"=="cp02" goto :mode_cp02

if "%BOOT_MODE%"=="2" goto :mode_cp01
if "%BOOT_MODE%"=="--cp01" goto :mode_cp01
if "%BOOT_MODE%"=="cp01" goto :mode_cp01

if "%BOOT_MODE%"=="3" goto :mode_cold
if "%BOOT_MODE%"=="--cold" goto :mode_cold
if "%BOOT_MODE%"=="cold" goto :mode_cold

if "%BOOT_MODE%"=="4" goto :mode_debug
if "%BOOT_MODE%"=="--debug" goto :mode_debug
if "%BOOT_MODE%"=="debug" goto :mode_debug

:mode_cp02
echo.
echo [*] Launching Fast Boot: Resuming from CP-02 Checkpoint...
set "EXTRA_ARGS=-loadvm cp02_rtr_token -qmp tcp:127.0.0.1:4444,server=on,wait=off"
goto :launch

:mode_cp01
echo.
echo [*] Launching Fast Boot: Resuming from CP-01 Checkpoint...
set "EXTRA_ARGS=-loadvm cp01_sf_stable -qmp tcp:127.0.0.1:4444,server=on,wait=off"
goto :launch

:mode_cold
echo.
echo [*] Launching Cold Boot: Starting Android 13 from scratch...
set "EXTRA_ARGS=-qmp tcp:127.0.0.1:4444,server=on,wait=off"
goto :launch

:mode_debug
echo.
echo [*] Launching Debug Boot: Cold Boot with GDB 1776 and QMP 4444...
set "EXTRA_ARGS=-gdb tcp::1776 -qmp tcp:127.0.0.1:4444,server=on,wait=off -S"
goto :launch

:launch
echo [*] QEMU Executable : "%QEMU_EXE%"
echo [*] DTB Image       : "%DTB_IMG%"
echo [*] eMMC Storage    : "%DISK_IMG%"
echo [*] Display Engine  : SDL (Native Window)
echo [*] Console Log     : "%CONSOLE_LOG%"
echo.
echo ================================================================================
echo Starting QEMU... (Close SDL display window or press Ctrl+C in terminal to stop)
echo ================================================================================
echo.

"%QEMU_EXE%" ^
    -L "%PC_BIOS%" ^
    -M virt,gic-version=3 ^
    -cpu cortex-a55 ^
    -smp 1 ^
    -m 4096 ^
    -device ramfb ^
    -display sdl ^
    -kernel "%KERNEL_IMG%" ^
    -initrd "%INITRD_IMG%" ^
    -dtb "%DTB_IMG%" ^
    -drive "file=%DISK_IMG%,if=sd,format=qcow2,id=emmc0" ^
    -append "init=/init nokaslr norandmaps androidboot.bootdevice=7c4000.sdhci console=ttyAMA0,115200 printk.devkmsg=on androidboot.debug_level=0x4f4c androidboot.selinux=permissive enforcing=0 androidboot.crypto_state=unencrypted androidboot.hardware=qcom androidboot.hardware.platform=sm6150 ro.crypto.state=unencrypted" ^
    -serial "file:%CONSOLE_LOG%" ^
    %EXTRA_ARGS%

goto :end

:error
echo.
echo [!] Launcher encountered an error. Please verify the file paths above.
echo.
pause
exit /b 1

:end
echo.
echo [*] QEMU emulator session ended.
pause
