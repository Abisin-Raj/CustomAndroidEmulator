import socket
import time
import subprocess
import sys
import struct
import os
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
    "-gdb", "tcp::1274,server,nowait",
    "-S",
    "-d", "guest_errors"
]

print("[Phase 7.6] Starting QEMU on port 1274 for vCPU0 start_kernel Progress Trace...", flush=True)
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
    s.connect(("127.0.0.1", 1274))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    # Run vCPU0 at full speed for 3.0s
    print("\n--- Progressing vCPU0 through start_kernel() ---", flush=True)


    # Allow vCPU0 to run at full speed for 3.0 seconds to pass DTB parsing and reach start_kernel()
    print("[vCPU0] Running vCPU0 at full speed (vCont;c:p01.01) for 3.0s to pass DTB parsing...", flush=True)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(3.0)

    # Interrupt / halt vCPU0
    send_rsp(s, "\x03")
    time.sleep(0.2)

    pc0 = get_reg(s, 32, "p01.01")
    sp0 = get_reg(s, 31, "p01.01")
    lr0 = get_reg(s, 30, "p01.01")

    mem_bytes = get_mem(s, pc0, 4, "p01.01")
    insn_val = struct.unpack('<I', mem_bytes)[0] if len(mem_bytes) == 4 else 0
    disasm_list = list(md.disasm(mem_bytes, pc0)) if len(mem_bytes) == 4 else []
    disasm_str = f"{disasm_list[0].mnemonic}\t{disasm_list[0].op_str}" if disasm_list else ""

    print(f"\n[vCPU0 Post-Run State]: PC=0x{pc0:016x} | SP=0x{sp0:016x} | LR=0x{lr0:016x} | Disasm: {disasm_str}", flush=True)


    time.sleep(0.5)
    s.close()
finally:
    proc.terminate()
    print("\n[Test Complete]", flush=True)
