$romDir = "build/launcher/roms/PixelExperience_violet-13.0-20240127-0711-OFFICIAL"
$bootImg = "$romDir/boot.img"
$tempKernel = "C:/Users/Public/temp_kernel_trace"
$qemuStdout = "C:/Users/Public/qemu_trace_stdout.log"
$qemuStderr = "C:/Users/Public/qemu_trace_stderr.log"
$gdbStdout = "C:/Users/Public/gdb_trace_stdout.log"
$gdbStderr = "C:/Users/Public/gdb_trace_stderr.log"

if (Test-Path $qemuStdout) { Remove-Item $qemuStdout -Force -ErrorAction SilentlyContinue }
if (Test-Path $qemuStderr) { Remove-Item $qemuStderr -Force -ErrorAction SilentlyContinue }
if (Test-Path $gdbStdout) { Remove-Item $gdbStdout -Force -ErrorAction SilentlyContinue }
if (Test-Path $gdbStderr) { Remove-Item $gdbStderr -Force -ErrorAction SilentlyContinue }

# 1. Extract raw kernel from boot.img
if (-not (Test-Path $bootImg)) {
    Write-Output "Error: boot.img not found at $bootImg"
    Exit 1
}
Write-Output "Extracting kernel from boot.img..."
$fs = [System.IO.File]::OpenRead($bootImg)
$fs.Seek(4096, 'Begin') | Out-Null
$buf = New-Object byte[] 17505611
$fs.Read($buf, 0, 17505611) | Out-Null
$fs.Close()
[System.IO.File]::WriteAllBytes($tempKernel, $buf)

# 2. Kill any existing QEMU process
Get-Process qemu-system-aarch64 -ErrorAction SilentlyContinue | Stop-Process -Force

# 3. Launch QEMU paused with GDB stub enabled, redirecting output
$qemu = "C:/msys64/mingw64/bin/qemu-system-aarch64.exe"
$cmdline = "androidboot.hardware=qcom androidboot.console=ttyMSM0 service_locator.enable=1 swiotlb=1 earlycon=pl011,0x09000000 loop.max_part=7 kpti=off androidboot.vbmeta.avb_version=1.0 buildvariant=userdebug console=ttyAMA0 printk.devkmsg=on nokaslr"

Write-Output "Launching QEMU (paused)..."
$qemuArgs = @("-M", "virt", "-cpu", "cortex-a76", "-m", "2048M", "-nographic", "-kernel", $tempKernel, "-append", "`"$cmdline`"", "-gdb", "tcp::1234", "-S")
$qemuProc = Start-Process -FilePath $qemu -ArgumentList $qemuArgs -NoNewWindow -RedirectStandardOutput $qemuStdout -RedirectStandardError $qemuStderr -PassThru

# Give QEMU half a second to bind
Start-Sleep -Milliseconds 800

# 4. Check if QEMU is listening on port 1234
Write-Output "Checking port 1234..."
$conn = Test-NetConnection -ComputerName "127.0.0.1" -Port 1234 -WarningAction SilentlyContinue
Write-Output "Port 1234 Open: $($conn.TcpTestSucceeded)"

if (-not $conn.TcpTestSucceeded) {
    Write-Output "Error: QEMU did not start or is not listening on port 1234. QEMU Error Log:"
    if (Test-Path $qemuStderr) { Get-Content $qemuStderr }
    $qemuProc | Stop-Process -Force
    Exit 1
}

# 5. Run GDB trace script in batch mode directly
$gdbScript = "$PSScriptRoot/trace_smc.gdb"
Write-Output "Running GDB trace with script..."
$gdb = "C:\msys64\mingw64\bin\gdb-multiarch.exe"

# Execute GDB and capture output
try {
    & $gdb -q -batch -x $gdbScript > $gdbStdout 2> $gdbStderr
} catch {
    Write-Output "GDB execution failed: $_"
}

# 6. Clean up
Write-Output "Cleaning up processes..."
$qemuProc | Stop-Process -Force
if (Test-Path $tempKernel) { Remove-Item $tempKernel -Force -ErrorAction SilentlyContinue }

# 7. Print GDB outputs
Write-Output "`n=== GDB Standard Output ==="
if (Test-Path $gdbStdout) { Get-Content $gdbStdout }
Write-Output "`n=== GDB Standard Error ==="
if (Test-Path $gdbStderr) { Get-Content $gdbStderr }

Write-Output "Finished."
