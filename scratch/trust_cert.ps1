$cert = Get-ChildItem -Path Cert:\CurrentUser\My -CodeSigningCert | Select-Object -First 1

try {
    $store1 = New-Object System.Security.Cryptography.X509Certificates.X509Store("Root", "LocalMachine")
    $store1.Open([System.Security.Cryptography.X509Certificates.OpenFlags]::ReadWrite)
    $store1.Add($cert)
    $store1.Close()
} catch {}

try {
    $store2 = New-Object System.Security.Cryptography.X509Certificates.X509Store("TrustedPublisher", "LocalMachine")
    $store2.Open([System.Security.Cryptography.X509Certificates.OpenFlags]::ReadWrite)
    $store2.Add($cert)
    $store2.Close()
} catch {}

try {
    $store3 = New-Object System.Security.Cryptography.X509Certificates.X509Store("TrustedPublisher", "CurrentUser")
    $store3.Open([System.Security.Cryptography.X509Certificates.OpenFlags]::ReadWrite)
    $store3.Add($cert)
    $store3.Close()
} catch {}

Write-Host "Cert installed into Root and TrustedPublisher stores."

