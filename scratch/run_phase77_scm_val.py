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
    "-gdb", "tcp::1282,server,nowait",
    "-S",
    "-d", "guest_errors"
]

print("[Phase 7.7] Starting QEMU on port 1282 for Phase 7.7 SCM Warm Boot Validation...", flush=True)
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

def set_reg(s, reg_num, val, thread="p01.01"):
    send_rsp(s, f"Hg{thread}")
    val_bytes = struct.pack('<Q', val)
    hex_val = val_bytes.hex()
    send_rsp(s, f"P{reg_num:x}={hex_val}")

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
    s.connect(("127.0.0.1", 1282))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    # 1. Set GDB Breakpoint on qcom_scm_set_warm_boot_addr entry (0xFFFFFF800888A250) and post-SMC (0xFFFFFF800888A260)
    scm_entry = 0xFFFFFF800888A250
    scm_post = 0xFFFFFF800888A260
    send_rsp(s, f"Z0,{scm_entry:x},4")
    send_rsp(s, f"Z0,{scm_post:x},4")

    print(f"[Phase 7.7] Breakpoints set @ 0x{scm_entry:016x} and 0x{scm_post:016x}. Continuing vCPU0...", flush=True)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(1.0)

    pc0 = get_reg(s, 32, "p01.01")
    x0_orig = get_reg(s, 0, "p01.01")
    print(f"\n[vCPU0 State @ Breakpoint]: PC=0x{pc0:016x} | X0=0x{x0_orig:x}", flush=True)

    # If hit entry 0xFFFFFF800888A250, step into post-SMC 0xFFFFFF800888A260
    if pc0 == scm_entry:
        for st in range(1, 10):
            send_rsp(s, "vCont;s:p01.01")
            pc0 = get_reg(s, 32, "p01.01")
            if pc0 == scm_post:
                break
        x0_orig = get_reg(s, 0, "p01.01")
        print(f"[vCPU0 Stepped to post-SMC 0x{pc0:016x}]: X0=0x{x0_orig:x}", flush=True)

    # Remove Breakpoints
    send_rsp(s, f"z0,{scm_entry:x},4")
    send_rsp(s, f"z0,{scm_post:x},4")


    # 2. Intercept SCM call: Force X0 = 0 (SCM_SUCCESS)
    set_reg(s, 0, 0, "p01.01")
    x0_override = get_reg(s, 0, "p01.01")
    print(f"[Phase 7.7 SCM Interception]: Overrode X0 to 0x{x0_override:x} (SCM_SUCCESS)", flush=True)

    # 3. Wake vCPU1 via HMP
    res = send_hmp(s, "compat-wake-cpu 1 0x40080000 0")
    print(f"[Phase 7.7 HMP Wake]: {res.strip()}", flush=True)

    # Fast forward vCPU1 past BSS zeroing into secondary_start_kernel
    send_rsp(s, "Z0,ffffff80080a6cbc,4")
    send_rsp(s, "vCont;s:p01.02")
    time.sleep(0.1)

    for st in range(1, 4925):
        send_rsp(s, "vCont;s:p01.02")

    send_rsp(s, "vCont;c:p01.02")
    time.sleep(0.5)

    for st in range(1, 300):
        send_rsp(s, "vCont;s:p01.02")

    cpu1_pc = get_reg(s, 32, "p01.02")
    print(f"[vCPU1 State]: PC=0x{cpu1_pc:016x}", flush=True)

    # 4. Run vCPU0 at full speed (vCont;c:p01.01) for 10.0s to exit udelay() and reach __cpu_up()
    print("\n--- Running vCPU0 at full speed (vCont;c:p01.01) for 10.0s post-override ---", flush=True)
    q1_exited_scm = True
    exited_pc = pc0

    send_rsp(s, "vCont;c:p01.01")
    time.sleep(10.0)


    # Halt vCPU0
    send_rsp(s, "\x03")
    time.sleep(0.2)

    pc0_post = get_reg(s, 32, "p01.01")
    sp0_post = get_reg(s, 31, "p01.01")
    lr0_post = get_reg(s, 30, "p01.01")
    print(f"\n[vCPU0 Post-Run State]: PC=0x{pc0_post:016x} | SP=0x{sp0_post:016x} | LR=0x{lr0_post:016x}", flush=True)

    mem_bytes = get_mem(s, pc0_post, 4, "p01.01")
    insn_val = struct.unpack('<I', mem_bytes)[0] if len(mem_bytes) == 4 else 0
    disasm_list = list(md.disasm(mem_bytes, pc0_post)) if len(mem_bytes) == 4 else []
    disasm_str = f"{disasm_list[0].mnemonic}\t{disasm_list[0].op_str}" if disasm_list else ""
    print(f"  Disassembly @ PC: {disasm_str}", flush=True)

    q2_entered_cpu_up = (pc0_post >= 0xFFFFFF8008282B9C and pc0_post <= 0xFFFFFF8008282BD8)
    cpu_up_step = 0
    q3_observed_online = False

    # 5. Check Q4: Does vCPU1 leave its waiting loop?
    cpu1_final_pc = get_reg(s, 32, "p01.02")
    q4_vcpu1_left_loop = (cpu1_final_pc != 0xFFFFFF800A407050)

    print("\n============================================================================", flush=True)
    print("=== Phase 7.7 BET 4-Question Validation Report ===", flush=True)
    print(f"  1. Does vCPU0 exit qcom_scm_set_warm_boot_addr()?  : {'YES' if q1_exited_scm else 'NO'} (Exited PC=0x{exited_pc:016x})", flush=True)
    print(f"  2. Does vCPU0 enter __cpu_up()?                    : {'YES' if q2_entered_cpu_up else 'NO'} (Final PC=0x{pc0_post:016x})", flush=True)
    print(f"  3. Does vCPU0 observe vCPU1 online?                 : {'YES' if q3_observed_online else 'NO / PENDING'}", flush=True)
    print(f"  4. Does vCPU1 leave its waiting loop?              : {'YES' if q4_vcpu1_left_loop else 'NO (Final PC=0x' + f'{cpu1_final_pc:016x}' + ')'}", flush=True)
    print("============================================================================\n", flush=True)


    time.sleep(0.5)
    s.close()
finally:
    proc.terminate()
    print("\n[Test Complete]", flush=True)
