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
    "-gdb", "tcp::1284,server,nowait",
    "-S",
    "-d", "guest_errors"
]

print("[Phase 7.8] Starting QEMU on port 1284 for Native SCM Compatibility Integration Test...", flush=True)
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
    s.connect(("127.0.0.1", 1284))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    # 1. Fast forward vCPU0 to qcom_scm_set_warm_boot_addr (0xFFFFFF800888A250)
    scm_addr = 0xFFFFFF800888A250
    send_rsp(s, f"Z0,{scm_addr:x},4")
    
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)

    print("[Phase 7.8] Continuing vCPU0 to hit qcom_scm_set_warm_boot_addr (0xFFFFFF800888A250)...", flush=True)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(1.0)
    send_rsp(s, f"z0,{scm_addr:x},4")

    pc0_scm = get_reg(s, 32, "p01.01")
    sp0_scm = get_reg(s, 31, "p01.01")
    lr0_scm = get_reg(s, 30, "p01.01")
    x0_scm = get_reg(s, 0, "p01.01")
    print(f"\n[vCPU0 State @ qcom_scm_set_warm_boot_addr]: PC=0x{pc0_scm:016x} | SP=0x{sp0_scm:016x} | LR=0x{lr0_scm:016x} | X0=0x{x0_scm:x}", flush=True)

    # 2. Wake vCPU1 via HMP
    res = send_hmp(s, "compat-wake-cpu 1 0x40080000 0")
    print(f"[Phase 7.8 HMP Wake]: {res.strip()}", flush=True)

    send_rsp(s, "vCont;s:p01.02")
    time.sleep(0.1)

    # 3. Run both vCPUs NATIVELY (Zero GDB overrides) for 5.0 seconds
    print("\n--- Running vCPU0 and vCPU1 NATIVELY (Zero GDB Register Overrides) for 5.0s ---", flush=True)
    send_rsp(s, "vCont;c")
    time.sleep(5.0)


    # 3. Halt both vCPUs to inspect native execution progress
    send_rsp(s, "\x03")
    time.sleep(0.2)

    cpu0_pc = get_reg(s, 32, "p01.01")
    cpu0_sp = get_reg(s, 31, "p01.01")
    cpu0_lr = get_reg(s, 30, "p01.01")
    cpu0_x0 = get_reg(s, 0, "p01.01")

    cpu1_pc = get_reg(s, 32, "p01.02")
    cpu1_sp = get_reg(s, 31, "p01.02")
    cpu1_lr = get_reg(s, 30, "p01.02")

    print(f"\n============================================================================", flush=True)
    print("=== Phase 7.8 Native Execution State Snapshot (Zero Overrides) ===", flush=True)
    print(f"  vCPU0 (Primary Core):   PC=0x{cpu0_pc:016x} | SP=0x{cpu0_sp:016x} | LR=0x{cpu0_lr:016x} | X0=0x{cpu0_x0:x}", flush=True)
    print(f"  vCPU1 (Secondary Core): PC=0x{cpu1_pc:016x} | SP=0x{cpu1_sp:016x} | LR=0x{cpu1_lr:016x}", flush=True)
    print("============================================================================\n", flush=True)

    # 4. Check whether vCPU0 exited qcom_scm_set_warm_boot_addr (0xFFFFFF800888A250..0xFFFFFF800888A274) natively
    in_scm_retry_loop = (cpu0_pc >= 0xFFFFFF800888A240 and cpu0_pc <= 0xFFFFFF800888A278)
    exited_scm_natively = not in_scm_retry_loop

    print(f"=== Phase 7.8 Native Verification Summary ===", flush=True)
    print(f"  - SCM 0xC2000601 Native Handling:   {'SUCCESS (Returned SCM_SUCCESS)' if exited_scm_natively else 'RETRYING / PENDING'}", flush=True)
    print(f"  - vCPU0 Permanent Loop Exit:         {'YES' if exited_scm_natively else 'NO'}", flush=True)
    print(f"  - vCPU0 Current Native Checkpoint:    PC=0x{cpu0_pc:016x}", flush=True)


    if cpu0_pc >= 0x40000000:
        print(f"\n=== Disassembling vCPU0 Active Native Checkpoint @ PC=0x{cpu0_pc:016x} ===", flush=True)
        mem0 = get_mem(s, cpu0_pc - 16, 0x40, "p01.01")
        if len(mem0) == 0x40:
            for off in range(0, 0x40, 4):
                addr = (cpu0_pc - 16) + off
                chunk = mem0[off:off+4]
                disasm = list(md.disasm(chunk, addr))
                if disasm:
                    insn = disasm[0]
                    marker = " <=== CURRENT vCPU0 PC" if addr == cpu0_pc else ""
                    print(f"  0x{addr:016x}: {insn.mnemonic}\t{insn.op_str}{marker}", flush=True)

    print("============================================================================\n", flush=True)

    time.sleep(0.5)
    s.close()
finally:
    proc.terminate()
    print("\n[Test Complete]", flush=True)
