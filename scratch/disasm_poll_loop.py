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
    "-gdb", "tcp::1300,server,nowait",
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
    s.connect(("127.0.0.1", 1300))
    s.sendall(b"+")

    # Fast-forward vCPU0 to 0xFFFFFF80080DC200
    bp_addr = 0xFFFFFF80080DC240
    send_rsp(s, f"Z0,{bp_addr:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.0)
    send_rsp(s, f"z0,{bp_addr:x},4")

    pc0 = get_reg(s, 32, "p01.01")
    sp0 = get_reg(s, 31, "p01.01")
    lr0 = get_reg(s, 30, "p01.01")

    # Dump all general purpose registers
    regs = {}
    for r in range(31):
        regs[f"X{r}"] = get_reg(s, r, "p01.01")

    print(f"\n============================================================================", flush=True)
    print(f"=== vCPU0 Register State @ Poll Loop (0x{pc0:016x}) ===", flush=True)
    for r in range(0, 31, 4):
        chunk = [f"X{i}=0x{regs[f'X{i}']:016x}" for i in range(r, min(r+4, 31))]
        print("  " + "  ".join(chunk), flush=True)
    print("============================================================================\n", flush=True)

    print(f"=== Disassembling Entire Poll Loop (0xFFFFFF80080DC200 to 0xFFFFFF80080DC280) ===", flush=True)
    mem = get_mem(s, 0xFFFFFF80080DC200, 0x80, "p01.01")
    if len(mem) == 0x80:
        for off in range(0, 0x80, 4):
            addr = 0xFFFFFF80080DC200 + off
            chunk = mem[off:off+4]
            disasm = list(md.disasm(chunk, addr))
            if disasm:
                insn = disasm[0]
                marker = " <=== CURRENT vCPU0 PC" if addr == pc0 else ""
                print(f"  0x{addr:016x}: {insn.mnemonic}\t{insn.op_str}{marker}", flush=True)

finally:
    s.close()
    proc.terminate()
