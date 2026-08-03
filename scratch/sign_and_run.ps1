$cert = Get-ChildItem -Path Cert:\CurrentUser\My -CodeSigningCert | Select-Object -First 1
$src  = ".\build_ninja\tests\execute_test_runner.exe"
$dest = ".\build_ninja\launcher\execute_test_runner.exe"

# Copy test runner to launcher dir and sign with DeveloperCodeSign
Copy-Item $src $dest -Force
Unblock-File -Path $dest -ErrorAction SilentlyContinue
Set-AuthenticodeSignature -FilePath $dest -Certificate $cert | Out-Null

# Set QEMU_DATA_DIR environment variable so QEMU resolves firmware ROMs
$env:QEMU_DATA_DIR = "C:\msys64\mingw64\share\qemu"

& $dest --run-real-vm
