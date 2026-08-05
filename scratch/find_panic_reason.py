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
    "-gdb", "tcp::1290,server,nowait",
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

def get_mem(s, addr, size, thread="p01.01"):
    send_rsp(s, f"Hg{thread}")
    resp = send_rsp(s, f"m{addr:x},{size}")
    for part in resp.split('$'):
        if part and '#' in part:
            raw = part.split('#')[0]
            if len(raw) >= size * 2:
                try:
                    return bytes.fromhex(raw[:size*2])
                except Exception:
                    pass
    return b""

md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect(("127.0.0.1", 1290))
    s.sendall(b"+")

    # Fast-forward vCPU0 to 0xFFFFFF80080DC27C (driver init return check)
    send_rsp(s, "Z0,ffffff80080dc27c,4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.0)
    send_rsp(s, "z0,ffffff80080dc27c,4")

    pc0 = get_reg(s, 32, "p01.01")
    x24 = get_reg(s, 24, "p01.01")
    x0 = get_reg(s, 0, "p01.01")
    x1 = get_reg(s, 1, "p01.01")

    val_aa4 = get_mem(s, x24 + 0xAA4, 4, "p01.01")
    status_val = struct.unpack('<I', val_aa4)[0] if len(val_aa4) == 4 else 0

    print(f"\n[vCPU0 Driver Status Check @ 0x{pc0:016x}]: X24=0x{x24:016x} | Status @ [X24+0xAA4]=0x{status_val:x}", flush=True)

    # Step to 0xFFFFFF80080DC29C (panic call) to read arguments X0, X1
    for st in range(1, 10):
        send_rsp(s, "vCont;s:p01.01")
        pc = get_reg(s, 32, "p01.01")
        if pc == 0xFFFFFF80080DC29C:
            break

    x0_panic = get_reg(s, 0, "p01.01")
    x1_panic = get_reg(s, 1, "p01.01")

    fmt_str = get_mem(s, x0_panic, 128, "p01.01").split(b'\x00')[0].decode(errors='ignore') if x0_panic >= 0xFFFFFF8000000000 else ""
    msg_str = get_mem(s, x1_panic, 128, "p01.01").split(b'\x00')[0].decode(errors='ignore') if x1_panic >= 0xFFFFFF8000000000 else ""

    print(f"\n============================================================================", flush=True)
    print(f"=== vCPU0 Kernel Panic Arguments @ 0xFFFFFF80080DC29C ===", flush=True)
    print(f"  Format (X0 = 0x{x0_panic:016x}): \"{fmt_str}\"", flush=True)
    print(f"  Message (X1 = 0x{x1_panic:016x}): \"{msg_str}\"", flush=True)
    print("============================================================================\n", flush=True)
finally:
    s.close()
    proc.terminate()
