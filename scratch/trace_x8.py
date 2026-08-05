import socket
import time
import subprocess
import os
import struct

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
    "-gdb", "tcp::1310,server,nowait",
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

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect(("127.0.0.1", 1310))
    s.sendall(b"+")

    # Fast-forward vCPU0 to 0xFFFFFF80080DC230
    bp_addr = 0xFFFFFF80080DC230
    send_rsp(s, f"Z0,{bp_addr:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.0)
    send_rsp(s, f"z0,{bp_addr:x},4")

    print("Fast-forwarded to 0xFFFFFF80080DC230. Starting step-by-step trace inside the loop...", flush=True)

    # Let's step vCPU0 and print the PC and instructions for 100 steps
    from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM
    md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
    
    def get_mem(s, addr, size):
        send_rsp(s, "Hgp01.01")
        resp = send_rsp(s, f"m{addr:x},{size}")
        for part in resp.split('$'):
            if part and '#' in part:
                raw = part.split('#')[0]
                if len(raw) >= size * 2:
                    return bytes.fromhex(raw[:size*2])
        return b""

    for step in range(50):
        pc = get_reg(s, 32, "p01.01")
        mem_bytes = get_mem(s, pc, 4)
        disasm_list = list(md.disasm(mem_bytes, pc)) if len(mem_bytes) == 4 else []
        disasm_str = f"{disasm_list[0].mnemonic}\t{disasm_list[0].op_str}" if disasm_list else ""
        
        # Print registers of interest
        x0 = get_reg(s, 0, "p01.01")
        x8 = get_reg(s, 8, "p01.01")
        w20 = get_reg(s, 20, "p01.01")
        
        print(f"[{step:02d}] PC=0x{pc:016x} | {disasm_str} | X0=0x{x0:x} X8=0x{x8:x} W20=0x{w20:x}", flush=True)
        send_rsp(s, "vCont;s:p01.01")

finally:
    s.close()
    proc.terminate()
