$cert = Get-ChildItem -Path Cert:\CurrentUser\My -CodeSigningCert | Select-Object -First 1
$exePath = "C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\build_ninja\tests\run_tests.exe"
Write-Host "Signing $exePath with certificate $($cert.Thumbprint)..."

Set-AuthenticodeSignature -FilePath $exePath -Certificate $cert
Write-Host "Signature applied. Running..."
& $exePath --run-real-vm
