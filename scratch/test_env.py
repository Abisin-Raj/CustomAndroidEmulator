import subprocess
import os
import sys
import time

my_env = os.environ.copy()
my_env["PATH"] = r"C:\msys64\mingw64\bin;C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + my_env.get("PATH", "")

qemu_bin = r"C:\qemu_source\build\qemu-system-aarch64.exe"




kernel_img = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\kernel.img"

cmd = [
    qemu_bin,
    "-machine", "virt",
    "-cpu", "cortex-a76,psci-conduit=1",
    "-smp", "2",
    "-m", "4096",
    "-nographic",
    "-kernel", kernel_img,
    "-append", "console=ttyAMA0 earlycon=pl011,0x09000000 androidboot.hardware=qcom nokaslr",
    "-gdb", "tcp::1245,server,nowait",
    "-S",
    "-d", "guest_errors"
]

print("[Test Env] Spawning QEMU with MSYS2 DLL environment...", flush=True)
proc = subprocess.Popen(cmd, env=my_env)
time.sleep(2.0)
print(f"[Test Env] Poll status after 2s: {proc.poll()}", flush=True)
if proc.poll() is None:
    print("[SUCCESS] QEMU is running cleanly in background!", flush=True)
    proc.terminate()
else:
    print(f"[FAIL] QEMU exited with code: {proc.poll()}", flush=True)
