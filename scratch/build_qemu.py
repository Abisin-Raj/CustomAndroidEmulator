import subprocess
import os
import sys

my_env = os.environ.copy()
my_env["PATH"] = r"C:\msys64\mingw64\bin;C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + my_env.get("PATH", "")

print("=== Building QEMU with Native SCM Handler (0xC2000601) ===", flush=True)

cmd = ["ninja", "libqemu-aarch64-softmmu.a.p/target_arm_tcg_psci.c.obj", "qemu-system-aarch64.exe"]


proc = subprocess.run(cmd, cwd=r"C:\qemu_source\build", env=my_env, capture_output=True, text=True)

print("STDOUT:", proc.stdout)
print("STDERR:", proc.stderr)
print(f"Build Exit Code: {proc.returncode}")

if proc.returncode == 0:
    print("[SUCCESS] qemu-system-aarch64.exe rebuilt with native SCM 0xC2000601 handler!", flush=True)
else:
    print("[FAILED] Compilation failed.", flush=True)
