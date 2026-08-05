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
    "-gdb", "tcp::1302,server,nowait",
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
    s.connect(("127.0.0.1", 1302))
    s.sendall(b"+")

    # Fast-forward vCPU0 to the target function @ 0xffffff80080dc4c0
    bp_addr = 0xffffff80080dc4c0
    send_rsp(s, f"Z0,{bp_addr:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.0)
    send_rsp(s, f"z0,{bp_addr:x},4")

    # Read registers
    pc = get_reg(s, 32, "p01.01")
    x0 = get_reg(s, 0, "p01.01")
    x1 = get_reg(s, 1, "p01.01")
    x2 = get_reg(s, 2, "p01.01")
    print(f"Reached PC = 0x{pc:016x}", flush=True)
    print(f"Registers at function entry: X0=0x{x0:x} X1=0x{x1:x} X2=0x{x2:x}", flush=True)

    print(f"=== Disassembling target function (0x{bp_addr:x}) ===", flush=True)
    mem = get_mem(s, bp_addr, 0x100, "p01.01")
    if len(mem) == 0x100:
        for off in range(0, 0x100, 4):
            addr = bp_addr + off
            chunk = mem[off:off+4]
            disasm = list(md.disasm(chunk, addr))
            if disasm:
                insn = disasm[0]
                print(f"  0x{addr:016x}: {insn.mnemonic}\t{insn.op_str}", flush=True)


finally:
    s.close()
    proc.terminate()
