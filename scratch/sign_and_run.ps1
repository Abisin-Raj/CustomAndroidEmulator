function Remove-Signature($filePath) {
    try {
        $bytes = [System.IO.File]::ReadAllBytes($filePath)
        $peOffset = [System.BitConverter]::ToUInt32($bytes, 0x3C)
        $magic = [System.BitConverter]::ToUInt16($bytes, $peOffset + 24)
        $secDirOffset = if ($magic -eq 0x20B) { $peOffset + 24 + 144 } else { $peOffset + 24 + 128 }
        [System.Array]::Clear($bytes, $secDirOffset, 8)
        [System.IO.File]::WriteAllBytes($filePath, $bytes)
    } catch {}
}

$src  = ".\build_ninja\tests\execute_test_runner.exe"
$dest = ".\build_ninja\launcher\execute_test_runner.exe"

Copy-Item $src $dest -Force
Remove-Signature $src
Remove-Signature $dest
Unblock-File -Path $dest -ErrorAction SilentlyContinue

$env:QEMU_DATA_DIR = "C:\msys64\mingw64\share\qemu"

& $dest --run-real-vm
