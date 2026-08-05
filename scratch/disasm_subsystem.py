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
    "-gdb", "tcp::1288,server,nowait",
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
    s.connect(("127.0.0.1", 1288))
    s.sendall(b"+")

    # Step once to unpause vCPU0
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)

    print("[vCPU0] Running vCPU0 natively for 3.0s...", flush=True)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(3.0)
    send_rsp(s, "\x03")
    time.sleep(0.2)


    pc0 = get_reg(s, 32, "p01.01")
    sp0 = get_reg(s, 31, "p01.01")
    lr0 = get_reg(s, 30, "p01.01")
    x0 = get_reg(s, 0, "p01.01")
    x1 = get_reg(s, 1, "p01.01")
    x5 = get_reg(s, 5, "p01.01")

    print(f"\n[vCPU0 Active State]: PC=0x{pc0:016x} | SP=0x{sp0:016x} | LR=0x{lr0:016x}", flush=True)
    print(f"  Registers: X0=0x{x0:x} X1=0x{x1:x} X5=0x{x5:x}", flush=True)

    print(f"=== Disassembling Subsystem Caller @ LR=0x{lr0:x} ===", flush=True)
    mem = get_mem(s, lr0 - 32, 0x50, "p01.01")
    if len(mem) == 0x50:
        for off in range(0, 0x50, 4):
            addr = (lr0 - 32) + off
            chunk = mem[off:off+4]
            disasm = list(md.disasm(chunk, addr))
            if disasm:
                insn = disasm[0]
                marker = " <=== RETURN CALLER LR" if addr == lr0 else ""
                print(f"  0x{addr:016x}: {insn.mnemonic}\t{insn.op_str}{marker}", flush=True)

    if x5 >= 0xFFFFFF8000000000:
        str_mem = get_mem(s, x5, 128, "p01.01")
        str_val = str_mem.split(b'\x00')[0].decode(errors='ignore')
        print(f"\n  [X5 String Context]: \"{str_val}\"", flush=True)

finally:
    s.close()
    proc.terminate()
