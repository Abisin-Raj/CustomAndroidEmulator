import socket
import time
import subprocess
import os
import struct
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM

qemu_bin = r"C:\qemu_source\build\qemu-system-aarch64.exe"
kernel_img = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\kernel.img"

my_env = os.environ.copy()
my_env["PATH"] = r"C:\msys64\mingw64\bin;C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + my_env.get("PATH", "")

cmd = [
    qemu_bin,
    "-machine", "virt",
    "-cpu", "cortex-a76,psci-conduit=1",
    "-smp", "2",
    "-m", "4096",
    "-nographic",
    "-kernel", kernel_img,
    "-append", "console=ttyAMA0 earlycon=pl011,0x09000000 androidboot.hardware=qcom nokaslr",
    "-gdb", "tcp::1266,server,nowait",
    "-S",
    "-d", "guest_errors"
]

proc = subprocess.Popen(cmd, env=my_env)
time.sleep(1.5)

def send_rsp(s, payload):
    checksum = sum(payload.encode('utf-8')) % 256
    pkt = f"${payload}#{checksum:02x}"
    s.sendall(pkt.encode('utf-8'))
    
    resp_accum = ""
    while True:
        try:
            chunk = s.recv(4096).decode(errors='ignore')
            if not chunk:
                break
            resp_accum += chunk
            if '$' in chunk and '#' in chunk:
                break
            if chunk == '+':
                continue
        except Exception:
            break
    s.sendall(b"+")
    return resp_accum

def send_hmp(s, hmp_cmd):
    hex_cmd = hmp_cmd.encode('utf-8').hex()
    resp = send_rsp(s, f"qRcmd,{hex_cmd}")
    out = ""
    for part in resp.split('$O'):
        if part:
            clean_hex = part.split('#')[0]
            try:
                out += bytes.fromhex(clean_hex).decode(errors='ignore')
            except Exception:
                pass
    return out

def get_reg(s, reg_num, thread="p01.01"):
    send_rsp(s, f"Hg{thread}")
    resp = send_rsp(s, f"p{reg_num:x}")
    for part in resp.split('$'):
        if part and '#' in part:
            raw = part.split('#')[0]
            if len(raw) >= 16:
                try:
                    val_bytes = bytes.fromhex(raw[:16])
                    return struct.unpack('<Q', val_bytes)[0]
                except Exception:
                    pass
    return 0

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect(("127.0.0.1", 1266))
    s.sendall(b"+")

    print("[Test SMP] Initial vCPU0 PC:", hex(get_reg(s, 32, "p01.01")), flush=True)

    # Wake CPU1
    res = send_hmp(s, "compat-wake-cpu 1 0x40080000 0")
    print(f"[Test SMP HMP]: {res.strip()}", flush=True)

    # Step vCPU0 once and vCPU1 once
    send_rsp(s, "vCont;s:p01.01")
    send_rsp(s, "vCont;s:p01.02")

    # Continue all threads for 1.0 second
    print("=== Running all threads concurrently for 1.0s ===", flush=True)
    send_rsp(s, "vCont;c")
    time.sleep(1.0)
    send_rsp(s, "\x03") # Interrupt
    time.sleep(0.2)

    cpu0_pc = get_reg(s, 32, "p01.01")
    cpu1_pc = get_reg(s, 32, "p01.02")

    print(f"\n[Test SMP Result]:", flush=True)
    print(f"  vCPU0 PC: 0x{cpu0_pc:016x}", flush=True)
    print(f"  vCPU1 PC: 0x{cpu1_pc:016x}", flush=True)

finally:
    s.close()
    proc.terminate()
