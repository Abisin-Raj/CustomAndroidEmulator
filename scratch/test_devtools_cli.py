import subprocess
import time
import os

qemu_bin = r"C:\qemu_source\build\qemu-system-aarch64.exe"
kernel_img = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\kernel.img"

my_env = os.environ.copy()
my_env["PATH"] = r"C:\msys64\mingw64\bin;C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + my_env.get("PATH", "")
# Add current directory to PYTHONPATH so devtools is discoverable
my_env["PYTHONPATH"] = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator;" + my_env.get("PYTHONPATH", "")

cmd = [
    qemu_bin,
    "-machine", "virt",
    "-cpu", "cortex-a76,psci-conduit=1",
    "-smp", "2",
    "-m", "4096",
    "-nographic",
    "-kernel", kernel_img,
    "-append", "console=ttyAMA0 earlycon=pl011,0x09000000 androidboot.hardware=qcom nokaslr",
    "-gdb", "tcp::1292,server,nowait",
    "-S",
    "-d", "guest_errors"
]

print("Starting QEMU on port 1292...", flush=True)
proc = subprocess.Popen(cmd, env=my_env)
time.sleep(1.5)

try:
    print("Running DevTools CLI run phase82...", flush=True)
    cli_cmd = [
        "python", "-m", "devtools.cli.devtools",
        "run", "phase82",
        "--config", "configs/pixel_experience.yaml"
    ]
    res = subprocess.run(cli_cmd, env=my_env, capture_output=True, text=True)
    print("CLI STDOUT:")
    print(res.stdout)
    print("CLI STDERR:")
    print(res.stderr)
    print(f"CLI Return Code: {res.returncode}")

finally:
    proc.terminate()
