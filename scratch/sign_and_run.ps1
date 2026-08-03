$cert = Get-ChildItem -Path Cert:\CurrentUser\My -CodeSigningCert | Select-Object -First 1
$exePath = "C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\build_ninja\tests\run_test_runner.exe"
Unblock-File -Path $exePath -ErrorAction SilentlyContinue
Set-AuthenticodeSignature -FilePath $exePath -Certificate $cert | Out-Null
Start-Sleep -Seconds 1
& $exePath --run-real-vm









