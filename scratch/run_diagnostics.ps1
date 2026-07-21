$romDir = "build/launcher/roms/PixelExperience_violet-13.0-20240127-0711-OFFICIAL"
$bootImg = "$romDir/boot.img"
$tempKernel = "C:/Users/Public/temp_kernel_diag"
$diagnosticLog = "docs/diagnostic_run_report.txt"
$qemuLog = "C:/Users/Public/qemu_diagnostic.log"
$qemuStdout = "C:/Users/Public/qemu_stdout.log"

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

# 2. Launch QEMU with tracing enabled
$qemu = "C:/msys64/mingw64/bin/qemu-system-aarch64.exe"
$cmdline = "androidboot.hardware=qcom androidboot.console=ttyMSM0 service_locator.enable=1 swiotlb=1 earlycon=pl011,0x09000000 loop.max_part=7 kpti=off androidboot.vbmeta.avb_version=1.0 buildvariant=userdebug console=ttyAMA0 printk.devkmsg=on nokaslr"

# Delete old logs
if (Test-Path $qemuLog) { Remove-Item $qemuLog -Force -ErrorAction SilentlyContinue }
if (Test-Path $qemuStdout) { Remove-Item $qemuStdout -Force -ErrorAction SilentlyContinue }
if (Test-Path $diagnosticLog) { Remove-Item $diagnosticLog -Force -ErrorAction SilentlyContinue }

Write-Output "Launching QEMU with tracing to $qemuLog and stdout to $qemuStdout..."
$job = Start-Job -ScriptBlock {
    param($q, $k, $c, $l, $o)
    & $q -machine virt,secure=on,gic-version=3 -cpu cortex-a76 -m 2048M -nographic -kernel $k -append $c -d guest_errors,unimp,int -D $l -monitor "tcp:127.0.0.1:4444,server,nowait" > $o 2>&1
} -ArgumentList $qemu, $tempKernel, $cmdline, $qemuLog, $qemuStdout

Write-Output "Waiting for boot execution..."
Start-Sleep -Seconds 5

$report = ""
$report += "=== QEMU RUNTIME DIAGNOSTIC PACK ===`n"
$report += "QEMU Version: 11.0.2`n"
$report += "Machine: virt`n"
$report += "CPU: cortex-a76`n"
$report += "RAM: 2048M`n"
$report += "Arguments: -kernel $tempKernel -append `"$cmdline`"`n`n"

try {
    Write-Output "Connecting to QEMU monitor..."
    $socket = New-Object System.Net.Sockets.TcpClient("127.0.0.1", 4444)
    $stream = $socket.GetStream()
    $writer = New-Object System.IO.StreamWriter($stream)
    $reader = New-Object System.IO.StreamReader($stream)

    Start-Sleep -Milliseconds 500
    $buffer = New-Object byte[] 8192
    $bytesRead = $stream.Read($buffer, 0, 8192)

    function Get-Response {
        Start-Sleep -Milliseconds 500
        $resp = ""
        while ($stream.DataAvailable) {
            $bytes = New-Object byte[] 8192
            $read = $stream.Read($bytes, 0, 8192)
            $resp += [System.Text.Encoding]::ASCII.GetString($bytes, 0, $read)
            Start-Sleep -Milliseconds 100
        }
        return $resp
    }

    # Query Status
    $writer.WriteLine("info status")
    $writer.Flush()
    $statusOut = Get-Response
    $report += "=== VM STATUS ===`n$statusOut`n"

    # Query Registers
    $writer.WriteLine("info registers")
    $writer.Flush()
    $regOut = Get-Response
    $report += "=== REGISTERS ===`n$regOut`n"

    $pc = ""
    $lr = ""
    if ($regOut -match 'PC=([0-9a-fA-F]+)') {
        $pc = $Matches[1]
        $report += "PC Address: 0x$pc`n"
        $writer.WriteLine("x /20i 0x$pc")
        $writer.Flush()
        $disasmOut = Get-Response
        $cleanDisasm = $disasmOut -replace "`e\[K", "" -replace "`e\[D", ""
        $report += "=== PC DISASSEMBLY ===`n$cleanDisasm`n"
    }

    # Disassemble around the stable Undefined Instruction target address (0xffffff800888a25c)
    $targetAddr = "0xffffff800888a25c"
    $report += "Disassembling target instruction at: $targetAddr`n"
    $writer.WriteLine("x /10i 0xffffff800888a250")
    $writer.Flush()
    $disasmOut = Get-Response
    $cleanDisasm = $disasmOut -replace "`e\[K", "" -replace "`e\[D", ""
    $report += "=== UNDEFINED INSTRUCTION DISASSEMBLY ===`n$cleanDisasm`n"

    # Test pmemsave for ramoops
    $ramoopsDump = "C:/Users/Public/ramoops_diagnostic.bin"
    if (Test-Path $ramoopsDump) { Remove-Item $ramoopsDump -Force -ErrorAction SilentlyContinue }
    $writer.WriteLine("pmemsave 0xa1600000 0x400000 $ramoopsDump")
    $writer.Flush()
    Start-Sleep -Milliseconds 1000
    Get-Response | Out-Null

    if (Test-Path $ramoopsDump) {
        $report += "=== RAMOOPS CAPTURE ===`n"
        $report += "Successfully saved 4MB persistent RAM buffer to $ramoopsDump.`n"
        $dumpBytes = [System.IO.File]::ReadAllBytes($ramoopsDump)
        $sig = [System.BitConverter]::ToUInt32($dumpBytes, 0)
        $report += "Ramoops Signature at offset 0: 0x" + $sig.ToString("x8") + "`n"
        if ($sig -eq 0x434d5347) {
            $report += "Found CSMG signature! Processing persistent logs...`n"
            $startOffset = [System.BitConverter]::ToUInt32($dumpBytes, 4)
            $sizeVal = [System.BitConverter]::ToUInt32($dumpBytes, 8)
            $report += "Write Pointer offset: $startOffset, Buffer size: $sizeVal`n"
        } else {
            $report += "No valid CSMG signature found in memory buffer (ramoops uninitialized).`n"
        }
    } else {
        $report += "=== RAMOOPS CAPTURE ===`nFailed to save ramoops memory dump (pmemsave command failed or address out of bounds).`n"
    }

    $writer.Close()
    $stream.Close()
    $socket.Close()
} catch {
    $report += "Monitor Connection Error: $_`n"
}

# Clean up QEMU job and stop processes first to release file locks
Stop-Job $job -ErrorAction SilentlyContinue
Remove-Job $job -ErrorAction SilentlyContinue

try {
    $qproc = Get-Process -Name qemu-system-aarch64 -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "*msys64*" }
    foreach ($p in $qproc) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }
} catch {}

# Capture QEMU console output
if (Test-Path $qemuStdout) {
    $report += "`n=== QEMU STDOUT/STDERR ===`n"
    $report += [System.IO.File]::ReadAllText($qemuStdout) + "`n"
}

if (Test-Path $tempKernel) { Remove-Item $tempKernel -Force -ErrorAction SilentlyContinue }

[System.IO.File]::WriteAllText($diagnosticLog, $report)
Write-Output "Diagnostics report written to $diagnosticLog"
