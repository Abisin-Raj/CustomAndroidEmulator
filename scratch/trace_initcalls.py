import socket
import time
import subprocess
import sys
import struct
import os
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM

qemu_bin = r"C:\qemu_source\build\qemu-system-aarch64.exe"
kernel_img = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\kernel.img"
sym_map_file = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\System.map.stub"

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
    "-gdb", "tcp::1296,server,nowait",
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
    s.connect(("127.0.0.1", 1296))
    s.sendall(b"+")

    # Fast-forward vCPU0 past free_area_init to do_initcalls loop (0xFFFFFF80080DC27C)
    init_chk = 0xFFFFFF80080DC27C
    send_rsp(s, f"Z0,{init_chk:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)

    print("[Phase 8.0] Fast-forwarding vCPU0 through start_kernel() to do_initcalls()...", flush=True)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.5)

    pc = get_reg(s, 32, "p01.01")
    print(f"\n[vCPU0 Reached do_initcalls Checkpoint]: PC=0x{pc:016x}", flush=True)

    # Wake vCPU1
    send_hmp(s, "compat-wake-cpu 1 0x40080000 0")
    time.sleep(0.1)

    # Step through 10 driver initcalls at 0xFFFFFF80080DC27C
    print("\n============================================================================", flush=True)
    print("=== Phase 8.0 Linux Boot Profiler: Active Initcall Traversal ===", flush=True)
    print("============================================================================\n", flush=True)

    for i in range(1, 11):
        pc = get_reg(s, 32, "p01.01")
        lr = get_reg(s, 30, "p01.01")
        w8_status = get_reg(s, 8, "p01.01") & 0xFFFFFFFF
        x24 = get_reg(s, 24, "p01.01")

        print(f"  [Initcall #{i:02d}] Active PC=0x{pc:016x} | LR=0x{lr:016x} | Driver Table Base=0x{x24:016x} | Init Status Reg=0x{w8_status:x}", flush=True)

        # Continue to next initcall iteration
        send_rsp(s, "vCont;c:p01.01")
        time.sleep(0.4)

    send_rsp(s, f"z0,{init_chk:x},4")

    pc_final = get_reg(s, 32, "p01.01")
    print(f"\n[Final Profile State]: vCPU0 PC=0x{pc_final:016x}", flush=True)

finally:
    s.close()
    proc.terminate()
