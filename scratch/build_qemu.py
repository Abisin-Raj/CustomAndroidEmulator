import subprocess
import os
import sys

my_env = os.environ.copy()
my_env["PATH"] = r"C:\msys64\mingw64\bin;C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + my_env.get("PATH", "")

build_dir = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidQEMU\build"
cmd = ["ninja", "-C", build_dir, "qemu-system-aarch64.exe"]

print(f"Running: {' '.join(cmd)}", flush=True)
proc = subprocess.run(cmd, env=my_env)
print(f"Build Exit Code: {proc.returncode}", flush=True)
sys.exit(proc.returncode)
