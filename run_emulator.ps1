<#
.SYNOPSIS
    Custom Android Emulator Launcher (Redmi Note 7 Pro / Snapdragon 675 / Android 13)
.DESCRIPTION
    Launches QEMU native SDL emulator with verified hardware paths, eMMC overlay,
    and fast-boot checkpoint restoration.
.PARAMETER Mode
    1 or 'fast'  : Instant Resume from CP-02 (SurfaceFlinger + BootAnimation active, ~2s) [Default]
    2 or 'cp01'  : Instant Resume from CP-01 (SurfaceFlinger initial stable)
    3 or 'cold'  : Full Android 13 boot from scratch (~2-3 min)
    4 or 'debug' : Cold boot with GDB stub (port 1776) and QMP (port 4444)
#>
param (
    [string]$Mode = ""
)

$ErrorActionPreference = "Stop"

Write-Host "================================================================================" -ForegroundColor Cyan
Write-Host " Qualcomm Snapdragon 675 / Redmi Note 7 Pro Android Emulator (QEMU Native SDL)" -ForegroundColor Cyan
Write-Host "================================================================================" -ForegroundColor Cyan
Write-Host ""

# -----------------------------------------------------------------------------
# Path Configuration
# -----------------------------------------------------------------------------
$QEMU_EXE   = "C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidQEMU\build\qemu-system-aarch64.exe"
$PC_BIOS    = "C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidQEMU\pc-bios"
$KERNEL_IMG = "C:\qemu_work\kernel.img"
$INITRD_IMG = "C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\boot_ramdisk.gz"
$DTB_IMG    = "C:\qemu_work\virt_spmi_compact_ion.dtb"
$DISK_IMG   = "C:\qemu_work\emmc_overlay.qcow2"
$CONSOLE_LOG= "C:\qemu_work\sf_console.log"

# -----------------------------------------------------------------------------
# Pre-Flight Verification
# -----------------------------------------------------------------------------
$paths = @{
    "QEMU Executable" = $QEMU_EXE
    "PC-BIOS Directory" = $PC_BIOS
    "Kernel Image" = $KERNEL_IMG
    "Ramdisk Image" = $INITRD_IMG
    "DTB Image" = $DTB_IMG
    "eMMC Disk Overlay" = $DISK_IMG
}

foreach ($item in $paths.GetEnumerator()) {
    if (-not (Test-Path $item.Value)) {
        Write-Host "[ERROR] $($item.Key) not found at:" -ForegroundColor Red
        Write-Host "        $($item.Value)" -ForegroundColor Red
        exit 1
    }
}

# -----------------------------------------------------------------------------
# Process Cleanup: Avoid Port / File Locking Conflicts
# -----------------------------------------------------------------------------
$existingQemu = Get-Process qemu-system-aarch64 -ErrorAction SilentlyContinue
if ($existingQemu) {
    Write-Host "[!] Previous QEMU instance detected (PID: $($existingQemu.Id))." -ForegroundColor Yellow
    Write-Host "[*] Terminating prior instance to free ports and eMMC locks..." -ForegroundColor Yellow
    Stop-Process -Name qemu-system-aarch64 -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 800
}

# -----------------------------------------------------------------------------
# Boot Mode Selection
# -----------------------------------------------------------------------------
if ([string]::IsNullOrWhiteSpace($Mode)) {
    Write-Host "  Select Boot Mode:" -ForegroundColor White
    Write-Host "    [1] Fast Boot  - Resume CP-02 (SurfaceFlinger + BootAnimation ready, ~2s) [DEFAULT]" -ForegroundColor Green
    Write-Host "    [2] Fast Boot  - Resume CP-01 (SurfaceFlinger stable, ~2s)" -ForegroundColor White
    Write-Host "    [3] Cold Boot  - Full Android 13 boot from scratch (~2-3 min)" -ForegroundColor White
    Write-Host "    [4] Debug Boot - Cold Boot with GDB (1776) and QMP (4444) stubs" -ForegroundColor Yellow
    Write-Host ""

    # Prompt with 5 second timeout
    Write-Host "  Choose mode [1-4] (Auto-selects [1] in 5s): " -NoNewline
    $timeoutSeconds = 5
    $selected = ""
    for ($i = 0; $i -lt $timeoutSeconds * 10; $i++) {
        if ([Console]::KeyAvailable) {
            $key = [Console]::ReadKey($true)
            $char = $key.KeyChar.ToString()
            if ($char -match '^[1-4]$') {
                $selected = $char
                Write-Host $char
                break
            }
        }
        Start-Sleep -Milliseconds 100
    }
    if ([string]::IsNullOrWhiteSpace($selected)) {
        $selected = "1"
        Write-Host "1 (Timeout auto-selected)" -ForegroundColor Green
    }
    $Mode = $selected
}

$extraArgs = @()

switch ($Mode.ToLower()) {
    { $_ -in "1", "fast", "--fast", "-fast", "cp02", "--cp02" } {
        Write-Host "`n[*] Launching Fast Boot: Resuming from CP-02 Checkpoint..." -ForegroundColor Green
        $extraArgs = @(
            "-loadvm", "cp02_rtr_token",
            "-qmp", "tcp:127.0.0.1:4444,server=on,wait=off"
        )
    }
    { $_ -in "2", "cp01", "--cp01", "-cp01" } {
        Write-Host "`n[*] Launching Fast Boot: Resuming from CP-01 Checkpoint..." -ForegroundColor Green
        $extraArgs = @(
            "-loadvm", "cp01_sf_stable",
            "-qmp", "tcp:127.0.0.1:4444,server=on,wait=off"
        )
    }
    { $_ -in "3", "cold", "--cold", "-cold" } {
        Write-Host "`n[*] Launching Cold Boot: Starting Android 13 from scratch..." -ForegroundColor Cyan
        $extraArgs = @(
            "-qmp", "tcp:127.0.0.1:4444,server=on,wait=off"
        )
    }
    { $_ -in "4", "debug", "--debug", "-debug" } {
        Write-Host "`n[*] Launching Debug Boot: Cold Boot with GDB (1776) and QMP (4444)..." -ForegroundColor Yellow
        $extraArgs = @(
            "-gdb", "tcp::1776",
            "-qmp", "tcp:127.0.0.1:4444,server=on,wait=off",
            "-S"
        )
    }
    default {
        Write-Host "`n[*] Unrecognized option '$Mode', defaulting to Fast Boot (CP-02)..." -ForegroundColor Yellow
        $extraArgs = @(
            "-loadvm", "cp02_rtr_token",
            "-qmp", "tcp:127.0.0.1:4444,server=on,wait=off"
        )
    }
}

Write-Host "[*] QEMU Executable : $QEMU_EXE"
Write-Host "[*] DTB Image       : $DTB_IMG"
Write-Host "[*] eMMC Storage    : $DISK_IMG"
Write-Host "[*] Display Engine  : SDL (Native Window)"
Write-Host "[*] Console Log     : $CONSOLE_LOG"
Write-Host ""
Write-Host "================================================================================" -ForegroundColor Cyan
Write-Host "Starting QEMU... (Close SDL display window or press Ctrl+C in terminal to stop)" -ForegroundColor Cyan
Write-Host "================================================================================" -ForegroundColor Cyan
Write-Host ""

$qemuArgs = @(
    "-L", $PC_BIOS,
    "-M", "virt,gic-version=3",
    "-cpu", "cortex-a55",
    "-smp", "1",
    "-m", "4096",
    "-device", "ramfb",
    "-display", "sdl",
    "-kernel", $KERNEL_IMG,
    "-initrd", $INITRD_IMG,
    "-dtb", $DTB_IMG,
    "-drive", "file=$DISK_IMG,if=sd,format=qcow2,id=emmc0",
    "-append", "init=/init nokaslr norandmaps androidboot.bootdevice=7c4000.sdhci console=ttyAMA0,115200 printk.devkmsg=on androidboot.debug_level=0x4f4c androidboot.selinux=permissive enforcing=0 androidboot.crypto_state=unencrypted androidboot.hardware=qcom androidboot.hardware.platform=sm6150 ro.crypto.state=unencrypted",
    "-serial", "file:$CONSOLE_LOG"
) + $extraArgs

& $QEMU_EXE @qemuArgs
