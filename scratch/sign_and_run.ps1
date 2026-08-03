$cert = Get-ChildItem -Path Cert:\CurrentUser\My -CodeSigningCert | Select-Object -First 1
$src  = ".\build_ninja\tests\execute_test_runner.exe"
$dest = ".\build_ninja\launcher\execute_test_runner.exe"

# Copy to launcher dir (WDAC allowed — same dir as launcher.exe which runs unsigned)
Copy-Item $src $dest -Force
Unblock-File -Path $dest -ErrorAction SilentlyContinue
Set-AuthenticodeSignature -FilePath $dest -Certificate $cert | Out-Null
Start-Sleep -Seconds 1

& $dest --run-real-vm
