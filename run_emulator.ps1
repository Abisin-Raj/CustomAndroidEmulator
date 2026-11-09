<#
.SYNOPSIS
    Custom Android Emulator Launcher (Redmi Note 7 Pro / Snapdragon 675 / Android 13)
.DESCRIPTION
    Launches QEMU native emulator with verified hardware paths, isolated session overlay,
    and fast-boot checkpoint restoration.
.PARAMETER Mode
    1 or 'fast'  : Instant Resume from CP-02 (SurfaceFlinger + BootAnimation active, ~2s) [Default]
    2 or 'cp01'  : Instant Resume from CP-01 [UNAVAILABLE - Snapshot destroyed]
    3 or 'cold'  : Full Android 13 boot from scratch (~2-3 min)
    4 or 'debug' : Cold boot with GDB stub (port 1776) and QMP (port 4444)
.PARAMETER Display
    sdl          : Native Win32 SDL window attached to RAMFB [Default]. Active UI scanout depends on SurfaceFlinger.
    none         : Disables QEMU host display frontend window. Does not guarantee guest Android framebuffer or SurfaceFlinger rendering.
.PARAMETER SkipBaseHashCheck
    Bypasses SHA-256 assertion and Read-Only attribute enforcement on emmc.img (Modes 3 & 4 only). Intended for developer workflows testing externally updated base images. Emulation still executes over a disposable QCOW2 overlay.
#>
param (
    [string]$Mode = "",
    [ValidateSet("sdl", "none")]
    [string]$Display = "sdl",
    [switch]$SkipBaseHashCheck
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
$QEMU_IMG   = "C:\msys64\mingw64\bin\qemu-img.exe"
$PC_BIOS    = "C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidQEMU\pc-bios"
$KERNEL_IMG = "C:\qemu_work\kernel_kgsl_d8_8.img"
$INITRD_IMG = "C:\qemu_work\boot_ramdisk_dri.gz"
$DTB_IMG    = "C:\qemu_work\virt_hw442.dtb"
$BASE_DISK  = "C:\qemu_work\emmc.img"
$EXPECTED_BASE_SHA    = "d76c56957056991eb057d2b40d17ec5ccbf7626d054112ba3e5b28f5469e919f"
$EXPECTED_OVERLAY_SHA = "0f6998614670dd597943827dbed7773ff2b428fa0e2134104b40d0e865ff686b"
$BASELINE_OV= "C:\qemu_work\emmc_overlay_d8_9_baseline.qcow2"
$CONSOLE_LOG= "C:\qemu_work\sf_console.log"

# -----------------------------------------------------------------------------
# Phase 1: Mode Normalization & Immediate Validation (Pure Logic, Zero IO)
# -----------------------------------------------------------------------------
if ([string]::IsNullOrWhiteSpace($Mode)) {
    Write-Host "  Select Boot Mode:" -ForegroundColor White
    Write-Host "    [1] Fast Boot  - Resume CP-02 (SurfaceFlinger + BootAnimation ready, ~2s) [DEFAULT]" -ForegroundColor Green
    Write-Host "    [2] Fast Boot  - Resume CP-01 [UNAVAILABLE - Snapshot destroyed]" -ForegroundColor DarkGray
    Write-Host "    [3] Cold Boot  - Full Android 13 boot from scratch (~2-3 min)" -ForegroundColor White
    Write-Host "    [4] Debug Boot - Cold Boot with GDB (1776) and QMP (4444) stubs" -ForegroundColor Yellow
    Write-Host ""

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

$canonicalMode = 0
switch ($Mode.ToLower()) {
    { $_ -in "1", "fast", "--fast", "-fast", "cp02", "--cp02" } {
        $canonicalMode = 1
    }
    { $_ -in "2", "cp01", "--cp01", "-cp01" } {
        Write-Host "`n[ERROR] Checkpoint CP-01 (cp01_sf_stable) is permanently unavailable." -ForegroundColor Red
        Write-Host "        The historical overlay (emmc_overlay.qcow2) contains 0 snapshots." -ForegroundColor Red
        Write-Host "        Please select Mode 1 (Fast Boot / CP-02) or Mode 3 (Cold Boot)." -ForegroundColor Yellow
        exit 1
    }
    { $_ -in "3", "cold", "--cold", "-cold" } {
        $canonicalMode = 3
    }
    { $_ -in "4", "debug", "--debug", "-debug" } {
        $canonicalMode = 4
    }
    default {
        Write-Host "`n[ERROR] Unrecognized mode '$Mode'." -ForegroundColor Red
        Write-Host "        Valid modes are: 1 (fast), 2 (cp01), 3 (cold), 4 (debug)." -ForegroundColor Yellow
        exit 1
    }
}

# Strict parameter safety guard: Fast Boot cannot bypass base hash verification
if ($canonicalMode -eq 1 -and $SkipBaseHashCheck) {
    Write-Host "`n[ERROR] -SkipBaseHashCheck cannot be used with Fast Boot (Mode 1)." -ForegroundColor Red
    Write-Host "        CP-02 checkpoint restoration strictly requires the verified baseline base disk." -ForegroundColor Red
    Write-Host "        To test with an unverified or upgraded base disk, use Mode 3 (Cold Boot)." -ForegroundColor Yellow
    exit 1
}

# -----------------------------------------------------------------------------
# Diagnostic & QEMU-IMG Validation Helpers
# -----------------------------------------------------------------------------
function Test-PortAdvisory([int]$port, [string]$purpose) {
    try {
        $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $port)
        $listener.Start()
        $listener.Stop()
    } catch {
        Write-Host "[WARNING] Port $port ($purpose) appears in use: $_" -ForegroundColor Yellow
    }
}

function Assert-FileIsReadOnly([string]$filePath) {
    $item = Get-Item $filePath
    if (-not $item.IsReadOnly) {
        Write-Host "[*] Applying Read-Only attribute on master artifact: $([System.IO.Path]::GetFileName($filePath))..." -ForegroundColor Cyan
        try {
            $item.IsReadOnly = $true
        } catch {
            Write-Host "[WARNING] Failed to set Read-Only attribute on ${filePath}: $_" -ForegroundColor Yellow
        }
    }
    $item = Get-Item $filePath
    if (-not $item.IsReadOnly) {
        Write-Host "[ERROR] Master artifact $filePath is not Read-Only and could not be write-protected!" -ForegroundColor Red
        return $false
    }
    return $true
}

function Test-FileUsageAdvisory([string]$filePath) {
    $fileName = [System.IO.Path]::GetFileName($filePath)
    $fullNormPath = [System.IO.Path]::GetFullPath($filePath).ToLowerInvariant()
    $activeQemu = Get-CimInstance Win32_Process -Filter "Name = 'qemu-system-aarch64.exe'" -ErrorAction SilentlyContinue
    if ($activeQemu) {
        foreach ($proc in $activeQemu) {
            if ($proc.CommandLine) {
                $cmd = $proc.CommandLine.ToLowerInvariant()
                if ($cmd.Contains($fileName.ToLowerInvariant()) -or $cmd.Contains($fullNormPath)) {
                    return @{ InUseSuspected = $true; Reason = "Process ID $($proc.ProcessId) references '$fileName' in command line." }
                }
            }
        }
    }
    # Advisory stream check: verifies read accessibility and absence of exclusive locks.
    # Note: On Windows, shared-read handle opens do not guarantee absence of other readers.
    try {
        $fileInfo = [System.IO.FileInfo]::new($filePath)
        $stream = $fileInfo.Open([System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
        $stream.Close()
    } catch {
        return @{ InUseSuspected = $true; Reason = "File locked or unreadable: $_" }
    }
    return @{ InUseSuspected = $false }
}

function Get-QemuImgInfoJson([string]$imgPath) {
    $pinfo = New-Object System.Diagnostics.ProcessStartInfo
    $pinfo.FileName = $QEMU_IMG
    $pinfo.Arguments = "info --output=json `"$imgPath`""
    $pinfo.RedirectStandardOutput = $true
    $pinfo.RedirectStandardError = $true
    $pinfo.UseShellExecute = $false
    $pinfo.CreateNoWindow = $true
    $proc = [System.Diagnostics.Process]::Start($pinfo)
    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()
    [System.Threading.Tasks.Task]::WaitAll($stdoutTask, $stderrTask)
    $proc.WaitForExit()
    return @{
        ExitCode = $proc.ExitCode
        Stdout   = $stdoutTask.Result
        Stderr   = $stderrTask.Result
    }
}

function Test-BackingChain([string]$ovPath, [string]$expectedBase) {
    $info = Get-QemuImgInfoJson $ovPath
    if ($info.ExitCode -ne 0) {
        return @{ Success = $false; Error = "qemu-img info failed (ExitCode $($info.ExitCode)): $($info.Stderr)" }
    }
    try {
        $json = $info.Stdout | ConvertFrom-Json
    } catch {
        return @{ Success = $false; Error = "Failed to parse JSON: $_" }
    }
    $rawBacking = $json.'backing-filename'
    if ([string]::IsNullOrWhiteSpace($rawBacking)) {
        return @{ Success = $false; Error = "No backing file recorded in overlay header." }
    }
    $expectedNorm = [System.IO.Path]::GetFullPath($expectedBase).ToLowerInvariant()
    $overlayDir = [System.IO.Path]::GetDirectoryName([System.IO.Path]::GetFullPath($ovPath))
    $resolvedBacking = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($overlayDir, $rawBacking)).ToLowerInvariant()
    if ($resolvedBacking -ne $expectedNorm) {
        return @{ Success = $false; Error = "Backing file mismatch. Expected '$expectedNorm', resolved '$resolvedBacking'" }
    }
    if (-not (Test-Path $resolvedBacking)) {
        return @{ Success = $false; Error = "Resolved backing file does not exist on disk: '$resolvedBacking'" }
    }
    if ($json.'backing-filename-format' -ne "raw") {
        return @{ Success = $false; Error = "Backing format is '$($json.'backing-filename-format')', expected 'raw'" }
    }
    return @{ Success = $true; ResolvedPath = $resolvedBacking }
}

function Test-SnapshotTag([string]$ovPath, [string]$tag) {
    $info = Get-QemuImgInfoJson $ovPath
    if ($info.ExitCode -ne 0) {
        Write-Host "[ERROR] qemu-img info failed on $ovPath (ExitCode $($info.ExitCode)): $($info.Stderr)" -ForegroundColor Red
        return $false
    }
    try {
        $json = $info.Stdout | ConvertFrom-Json
    } catch {
        Write-Host "[ERROR] Failed to parse JSON output for ${ovPath}: $_" -ForegroundColor Red
        return $false
    }
    if ($json.snapshots) {
        foreach ($s in $json.snapshots) {
            if ($s.name -eq $tag) {
                return $true
            }
        }
    }
    return $false
}

# -----------------------------------------------------------------------------
# Phase 2: Targeted Pre-Flight Verification
# -----------------------------------------------------------------------------
$foundationalPaths = @{
    "QEMU Executable"   = $QEMU_EXE
    "QEMU-IMG Tool"     = $QEMU_IMG
    "PC-BIOS Directory" = $PC_BIOS
    "Kernel Image"      = $KERNEL_IMG
    "Ramdisk Image"     = $INITRD_IMG
    "DTB Image"         = $DTB_IMG
    "Base eMMC Disk"    = $BASE_DISK
}
if ($canonicalMode -eq 1) {
    $foundationalPaths["Master Baseline Overlay"] = $BASELINE_OV
}
foreach ($item in $foundationalPaths.GetEnumerator()) {
    if (-not (Test-Path $item.Value)) {
        Write-Host "[ERROR] $($item.Key) not found at:" -ForegroundColor Red
        Write-Host "        $($item.Value)" -ForegroundColor Red
        exit 1
    }
}

# -----------------------------------------------------------------------------
# Phase 3: Mode-Specific Execution Setup
# -----------------------------------------------------------------------------
$extraArgs = @()

if ($canonicalMode -eq 1) {
    Write-Host "`n[*] Launching Fast Boot: Resuming from CP-02 Checkpoint..." -ForegroundColor Green

    # 1. Advisory port preflight
    Test-PortAdvisory 4444 "QMP Monitor"

    # 2. Master baseline preflight: Unconditional dual-artifact hash assertion
    $preMasterHash = (Get-FileHash -Algorithm SHA256 $BASELINE_OV).Hash.ToLower()
    if ($preMasterHash -ne $EXPECTED_OVERLAY_SHA) {
        Write-Host "[ERROR] Baseline overlay hash mismatch!" -ForegroundColor Red
        Write-Host "        Expected: $EXPECTED_OVERLAY_SHA" -ForegroundColor Red
        Write-Host "        Actual:   $preMasterHash" -ForegroundColor Red
        Write-Host "        Aborting Fast Boot to prevent corruption or invalid snapshot load." -ForegroundColor Red
        exit 1
    }
    $curBaseHash = (Get-FileHash -Algorithm SHA256 $BASE_DISK).Hash.ToLower()
    if ($curBaseHash -ne $EXPECTED_BASE_SHA) {
        Write-Host "[ERROR] Base disk hash mismatch!" -ForegroundColor Red
        Write-Host "        Expected: $EXPECTED_BASE_SHA" -ForegroundColor Red
        Write-Host "        Actual:   $curBaseHash" -ForegroundColor Red
        Write-Host "        Fast Boot requires the verified baseline base disk. Checkpoint restoration cannot bypass base hash verification." -ForegroundColor Red
        exit 1
    }

    # 3. Read-Only attribute safeguard on master artifacts
    if (-not (Assert-FileIsReadOnly $BASELINE_OV)) { exit 1 }
    if (-not (Assert-FileIsReadOnly $BASE_DISK)) { exit 1 }

    # 4. Check for active QEMU processes referencing master artifacts
    $lockCheckOv = Test-FileUsageAdvisory $BASELINE_OV
    if ($lockCheckOv.InUseSuspected) {
        Write-Host "[ERROR] Baseline overlay advisory check failed: $($lockCheckOv.Reason)" -ForegroundColor Red
        exit 1
    }
    $lockCheckBase = Test-FileUsageAdvisory $BASE_DISK
    if ($lockCheckBase.InUseSuspected) {
        Write-Host "[ERROR] Base disk advisory check failed: $($lockCheckBase.Reason)" -ForegroundColor Red
        exit 1
    }

    # 5. Generate unique session overlay path
    $sessionId = (Get-Date).ToString("yyyyMMdd_HHmmss") + "_" + [System.Guid]::NewGuid().ToString("N").Substring(0, 6)
    $sessionOverlay = "C:\qemu_work\emmc_overlay_fastboot_session_$sessionId.qcow2"

    # 6. Create isolated session copy (Master overlay remains strictly offline)
    Write-Host "[*] Creating unique session overlay: $sessionOverlay..." -ForegroundColor Cyan
    Copy-Item $BASELINE_OV $sessionOverlay
    (Get-Item $sessionOverlay).IsReadOnly = $false

    # 7. Three-way hash concurrency check
    $postMasterHash = (Get-FileHash -Algorithm SHA256 $BASELINE_OV).Hash.ToLower()
    $copyHash = (Get-FileHash -Algorithm SHA256 $sessionOverlay).Hash.ToLower()
    if ($preMasterHash -ne $postMasterHash) {
        Write-Host "[ERROR] Master baseline overlay mutated during copy operation!" -ForegroundColor Red
        Remove-Item $sessionOverlay -Force -ErrorAction SilentlyContinue
        exit 1
    }
    if ($copyHash -ne $postMasterHash) {
        Write-Host "[ERROR] Session overlay copy fidelity check failed!" -ForegroundColor Red
        Remove-Item $sessionOverlay -Force -ErrorAction SilentlyContinue
        exit 1
    }

    # 8. Independent validation of snapshot tag directly on copied session overlay
    if (-not (Test-SnapshotTag $sessionOverlay "cp02_sf_stable")) {
        Write-Host "[ERROR] Snapshot tag 'cp02_sf_stable' missing from copied session overlay!" -ForegroundColor Red
        Remove-Item $sessionOverlay -Force -ErrorAction SilentlyContinue
        exit 1
    }

    # 9. Independent validation of normalized backing chain directly on copied session overlay
    $chainRes = Test-BackingChain $sessionOverlay $BASE_DISK
    if (-not $chainRes.Success) {
        Write-Host "[ERROR] Session overlay backing chain validation failed: $($chainRes.Error)" -ForegroundColor Red
        Remove-Item $sessionOverlay -Force -ErrorAction SilentlyContinue
        exit 1
    }

    $DISK_IMG = $sessionOverlay
    $extraArgs = @(
        "-loadvm", "cp02_sf_stable",
        "-qmp", "tcp:127.0.0.1:4444,server=on,wait=off"
    )
} elseif ($canonicalMode -eq 3) {
    Write-Host "`n[*] Launching Cold Boot: Starting Android 13 from scratch..." -ForegroundColor Cyan
    # 1. Advisory port preflight
    Test-PortAdvisory 4444 "QMP Monitor"

    # 2. Base disk cryptographic integrity verification (with documented developer bypass)
    Write-Host "[*] Validating base disk SHA-256 integrity..." -ForegroundColor Cyan
    $curBaseHash = (Get-FileHash -Algorithm SHA256 $BASE_DISK).Hash.ToLower()
    if ($curBaseHash -ne $EXPECTED_BASE_SHA) {
        if ($SkipBaseHashCheck) {
            Write-Host "[WARNING] Base disk hash differs from approved baseline ($EXPECTED_BASE_SHA)." -ForegroundColor Yellow
            Write-Host "          -SkipBaseHashCheck active: continuing with unverified base disk ($curBaseHash)." -ForegroundColor Yellow
        } else {
            Write-Host "[ERROR] Base disk hash mismatch! Expected $EXPECTED_BASE_SHA, found $curBaseHash" -ForegroundColor Red
            Write-Host "        If you intentionally updated emmc.img, pass -SkipBaseHashCheck to bypass." -ForegroundColor Yellow
            exit 1
        }
    }

    # 3. Read-Only attribute safeguard on base disk (bypassed if -SkipBaseHashCheck for developer workflow)
    if ($SkipBaseHashCheck) {
        Write-Host "[WARNING] -SkipBaseHashCheck active: base disk Read-Only attribute enforcement bypassed for developer workflow." -ForegroundColor Yellow
    } else {
        if (-not (Assert-FileIsReadOnly $BASE_DISK)) { exit 1 }
    }

    $lockCheck = Test-FileUsageAdvisory $BASE_DISK
    if ($lockCheck.InUseSuspected) {
        Write-Host "[ERROR] Base disk advisory check failed: $($lockCheck.Reason)" -ForegroundColor Red
        exit 1
    }

    # 4. Generate unique disposable overlay path
    $sessionId = (Get-Date).ToString("yyyyMMdd_HHmmss") + "_" + [System.Guid]::NewGuid().ToString("N").Substring(0, 6)
    $disposableOverlay = "C:\qemu_work\emmc_overlay_coldboot_disposable_$sessionId.qcow2"

    # 5. Create fresh disposable overlay backed by base disk
    Write-Host "[*] Creating unique disposable overlay: $disposableOverlay..." -ForegroundColor Cyan
    & $QEMU_IMG create -f qcow2 -b $BASE_DISK -F raw $disposableOverlay
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] qemu-img create failed with exit code $LASTEXITCODE" -ForegroundColor Red
        exit 1
    }

    # 6. Normalized backing chain validation
    $chainRes = Test-BackingChain $disposableOverlay $BASE_DISK
    if (-not $chainRes.Success) {
        Write-Host "[ERROR] Disposable overlay backing chain validation failed: $($chainRes.Error)" -ForegroundColor Red
        Remove-Item $disposableOverlay -Force -ErrorAction SilentlyContinue
        exit 1
    }

    $DISK_IMG = $disposableOverlay
    $extraArgs = @(
        "-qmp", "tcp:127.0.0.1:4444,server=on,wait=off"
    )
} elseif ($canonicalMode -eq 4) {
    Write-Host "`n[*] Launching Debug Boot: Cold Boot with GDB (1776) and QMP (4444)..." -ForegroundColor Yellow
    # 1. Advisory port preflight
    Test-PortAdvisory 4444 "QMP Monitor"
    Test-PortAdvisory 1776 "GDB Stub"

    # 2. Base disk cryptographic integrity verification (with documented developer bypass)
    Write-Host "[*] Validating base disk SHA-256 integrity..." -ForegroundColor Cyan
    $curBaseHash = (Get-FileHash -Algorithm SHA256 $BASE_DISK).Hash.ToLower()
    if ($curBaseHash -ne $EXPECTED_BASE_SHA) {
        if ($SkipBaseHashCheck) {
            Write-Host "[WARNING] Base disk hash differs from approved baseline ($EXPECTED_BASE_SHA)." -ForegroundColor Yellow
            Write-Host "          -SkipBaseHashCheck active: continuing with unverified base disk ($curBaseHash)." -ForegroundColor Yellow
        } else {
            Write-Host "[ERROR] Base disk hash mismatch! Expected $EXPECTED_BASE_SHA, found $curBaseHash" -ForegroundColor Red
            Write-Host "        If you intentionally updated emmc.img, pass -SkipBaseHashCheck to bypass." -ForegroundColor Yellow
            exit 1
        }
    }

    # 3. Read-Only attribute safeguard on base disk (bypassed if -SkipBaseHashCheck for developer workflow)
    if ($SkipBaseHashCheck) {
        Write-Host "[WARNING] -SkipBaseHashCheck active: base disk Read-Only attribute enforcement bypassed for developer workflow." -ForegroundColor Yellow
    } else {
        if (-not (Assert-FileIsReadOnly $BASE_DISK)) { exit 1 }
    }

    $lockCheck = Test-FileUsageAdvisory $BASE_DISK
    if ($lockCheck.InUseSuspected) {
        Write-Host "[ERROR] Base disk advisory check failed: $($lockCheck.Reason)" -ForegroundColor Red
        exit 1
    }

    # 4. Generate unique disposable overlay path
    $sessionId = (Get-Date).ToString("yyyyMMdd_HHmmss") + "_" + [System.Guid]::NewGuid().ToString("N").Substring(0, 6)
    $disposableOverlay = "C:\qemu_work\emmc_overlay_coldboot_disposable_$sessionId.qcow2"

    # 5. Create fresh disposable overlay backed by base disk
    Write-Host "[*] Creating unique disposable overlay: $disposableOverlay..." -ForegroundColor Cyan
    & $QEMU_IMG create -f qcow2 -b $BASE_DISK -F raw $disposableOverlay
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] qemu-img create failed with exit code $LASTEXITCODE" -ForegroundColor Red
        exit 1
    }

    # 6. Normalized backing chain validation
    $chainRes = Test-BackingChain $disposableOverlay $BASE_DISK
    if (-not $chainRes.Success) {
        Write-Host "[ERROR] Disposable overlay backing chain validation failed: $($chainRes.Error)" -ForegroundColor Red
        Remove-Item $disposableOverlay -Force -ErrorAction SilentlyContinue
        exit 1
    }

    $DISK_IMG = $disposableOverlay
    $extraArgs = @(
        "-gdb", "tcp::1776",
        "-qmp", "tcp:127.0.0.1:4444,server=on,wait=off",
        "-S"
    )
}

Write-Host "[*] QEMU Executable : $QEMU_EXE"
Write-Host "[*] DTB Image       : $DTB_IMG"
Write-Host "[*] eMMC Storage    : $DISK_IMG"
Write-Host "[*] Display Engine  : $Display"
if ($Display -eq "sdl") {
    Write-Host "[NOTE] SDL display window attached to RAMFB. Snapshot restoration resumes CPU execution; active UI scanout depends on SurfaceFlinger frame commits." -ForegroundColor Yellow
} elseif ($Display -eq "none") {
    Write-Host "[NOTE] -Display none disables QEMU display frontend window. It does not establish that guest Android framebuffer or SurfaceFlinger is rendering." -ForegroundColor Yellow
}
Write-Host "[*] Console Log     : $CONSOLE_LOG"
Write-Host ""
Write-Host "================================================================================" -ForegroundColor Cyan
Write-Host "Starting QEMU... (Press Ctrl+C in terminal or close window to stop)" -ForegroundColor Cyan
Write-Host "================================================================================" -ForegroundColor Cyan
Write-Host ""

$qemuArgs = @(
    "-L", $PC_BIOS,
    "-M", "virt,gic-version=3",
    "-cpu", "cortex-a55",
    "-smp", "1",
    "-m", "4096",
    "-device", "ramfb",
    "-display", $Display,
    "-kernel", $KERNEL_IMG,
    "-initrd", $INITRD_IMG,
    "-dtb", $DTB_IMG,
    "-drive", "file=$DISK_IMG,if=sd,format=qcow2,id=emmc0",
    "-append", "init=/init nokaslr norandmaps androidboot.bootdevice=7c4000.sdhci console=ttyAMA0,115200 printk.devkmsg=on androidboot.debug_level=0x4f4c androidboot.selinux=permissive enforcing=0 androidboot.crypto_state=unencrypted androidboot.hardware=qcom androidboot.hardware.platform=sm6150 ro.crypto.state=unencrypted",
    "-serial", "file:$CONSOLE_LOG"
) + $extraArgs

$launchStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
& $QEMU_EXE @qemuArgs
$qemuExitCode = $LASTEXITCODE
$launchStopwatch.Stop()

if ($qemuExitCode -ne 0) {
    $elapsedSec = $launchStopwatch.Elapsed.TotalSeconds.ToString("F1")
    Write-Host "`n[!] QEMU terminated with exit code $qemuExitCode (Elapsed: ${elapsedSec}s)." -ForegroundColor Yellow
    Write-Host "    [!] Post-Mortem Diagnostic Assessment (Heuristic Estimate):" -ForegroundColor Yellow
    if ($launchStopwatch.Elapsed.TotalSeconds -lt 4) {
        Write-Host "        Likely Early Initialization Failure (elapsed < 4s)." -ForegroundColor Yellow
        Write-Host "        Common causes: TCP port conflict (QMP 4444 / GDB 1776), CLI argument error, or missing/corrupted assets." -ForegroundColor Gray
    } else {
        Write-Host "        Likely Guest Runtime Termination (elapsed >= 4s)." -ForegroundColor Yellow
        Write-Host "        Common causes: Guest kernel panic, hardware fault, or interactive termination." -ForegroundColor Gray
    }
    Write-Host "        Notice: Elapsed time is heuristic only. Check console log and overlay for definitive diagnosis." -ForegroundColor Gray
    Write-Host "        Diagnostic overlay preserved at: $DISK_IMG" -ForegroundColor Cyan
    Write-Host "        Inspect overlay: & `"$QEMU_IMG`" info `"$DISK_IMG`"" -ForegroundColor Gray
    Write-Host "        Review log     : $CONSOLE_LOG" -ForegroundColor Gray
} else {
    Write-Host "`n[*] QEMU session terminated normally (exit code 0)." -ForegroundColor Green
    Write-Host "    Session overlay preserved: $DISK_IMG" -ForegroundColor Cyan
}
