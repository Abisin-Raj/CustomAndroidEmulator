"""
Phase 8.2b: Verify full kernel continuation after forcing X26 past threshold.
Confirmed: Loop exits when X26 >= 5000. Now check if kernel continues to next initcall
or hits another blocker immediately.
"""

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
    qemu_bin, "-machine", "virt",
    "-cpu", "cortex-a76,psci-conduit=1",
    "-smp", "2", "-m", "4096", "-nographic",
    "-kernel", kernel_img,
    "-append", "console=ttyAMA0 earlycon=pl011,0x09000000 androidboot.hardware=qcom nokaslr",
    "-gdb", "tcp::1326,server,nowait", "-S", "-d", "guest_errors"
]

proc = subprocess.Popen(cmd, env=my_env)
time.sleep(1.5)

def send_rsp(s, payload):
    checksum = sum(payload.encode('utf-8')) % 256
    s.sendall(f"${payload}#{checksum:02x}".encode('utf-8'))
    resp = ""
    while True:
        try:
            chunk = s.recv(4096).decode(errors='ignore')
            if not chunk: break
            resp += chunk
            if '$' in chunk and '#' in chunk: break
            if chunk == '+': continue
        except Exception: break
    s.sendall(b"+")
    return resp

def send_hmp(s, hmp_cmd):
    hex_cmd = hmp_cmd.encode('utf-8').hex()
    resp = send_rsp(s, f"qRcmd,{hex_cmd}")
    out = ""
    for part in resp.split('$O'):
        if part:
            try: out += bytes.fromhex(part.split('#')[0]).decode(errors='ignore')
            except Exception: pass
    return out

def get_reg(s, r, thread="p01.01"):
    send_rsp(s, f"Hg{thread}")
    resp = send_rsp(s, f"p{r:x}")
    for part in resp.split('$'):
        if part and '#' in part:
            raw = part.split('#')[0]
            if len(raw) >= 16:
                try: return struct.unpack('<Q', bytes.fromhex(raw[:16]))[0]
                except Exception: pass
    return 0

def set_reg(s, r, val, thread="p01.01"):
    send_rsp(s, f"Hg{thread}")
    val_hex = struct.pack('<Q', val & 0xFFFFFFFFFFFFFFFF).hex()
    return send_rsp(s, f"P{r:x}={val_hex}")

md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10.0)
    s.connect(("127.0.0.1", 1326))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    print("\n================================================================", flush=True)
    print("=== Phase 8.2b: Full Kernel Continuation Trace ===", flush=True)
    print("================================================================\n", flush=True)

    # Fast-forward to blr x8 at 0xFFFFFF80080DC230
    bp_blr  = 0xFFFFFF80080DC230
    bp_add  = 0xFFFFFF80080DC234

    send_rsp(s, f"Z0,{bp_blr:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.5)
    send_rsp(s, f"z0,{bp_blr:x},4")

    # Wake CPU1
    send_hmp(s, "compat-wake-cpu 1 0x40080000 0")
    time.sleep(0.1)

    # Single-step through blr x8
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.05)

    # Continue to callback return at 0xFFFFFF80080DC234
    send_rsp(s, f"Z0,{bp_add:x},4")
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(0.5)
    send_rsp(s, f"z0,{bp_add:x},4")

    pc = get_reg(s, 32)
    print(f"At callback return site: PC=0x{pc:016x}", flush=True)

    # Override X0 = 5001
    set_reg(s, 0, 5001)
    # Single-step add x26, x0, x26
    send_rsp(s, "vCont;s:p01.01"); time.sleep(0.05)
    # Single-step add x23, x26, #0xc8
    send_rsp(s, "vCont;s:p01.01"); time.sleep(0.05)

    x26 = get_reg(s, 26)
    print(f"X26 = {x26} (loop threshold = 5000) — loop will exit", flush=True)

    # Now let the loop exit at 0xFFFFFF80080DC268 (b instruction, not b.lt)
    # Then continue to next initcall at 0xFFFFFF80080DC27C
    # and from there further into 0xFFFFFF80080DC370 (bl to next initcall fn)

    # Set checkpoints at key addresses:
    milestones = [
        (0xFFFFFF80080DC27C, "do_initcalls check (taint check)"),
        (0xFFFFFF80080DC370, "next initcall dispatch (bl x0)"),
        (0xFFFFFF80080DC374, "initcall return"),
        (0xFFFFFF80080DC008, "do_initcalls loop top"),
    ]

    for addr, label in milestones:
        send_rsp(s, f"Z0,{addr:x},4")

    print(f"\n[Continuing kernel execution — watching for next milestones...]", flush=True)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.0)

    pc_hit = get_reg(s, 32)
    pc_sym = next((label for addr, label in milestones if addr == pc_hit), "unknown")

    for addr, label in milestones:
        send_rsp(s, f"z0,{addr:x},4")

    print(f"\n  vCPU0 stopped at: 0x{pc_hit:016x} ({pc_sym})", flush=True)
    print(f"  vCPU1 PC: 0x{get_reg(s, 32, 'p01.02'):016x}", flush=True)

    # Now let the kernel run for 3 more seconds and see what happens
    print(f"\n[Continuing for 3s to observe kernel progression...]", flush=True)
    # Set a broad net of checkpoints
    for addr, _ in milestones:
        send_rsp(s, f"Z0,{addr:x},4")

    for i in range(6):
        send_rsp(s, "vCont;c:p01.01")
        time.sleep(0.5)
        pc_hit = get_reg(s, 32)
        pc_sym = next((label for addr, label in milestones if addr == pc_hit), f"0x{pc_hit:016x}")
        print(f"  [{i*0.5:.1f}s] vCPU0 at: {pc_sym}", flush=True)

    for addr, _ in milestones:
        send_rsp(s, f"z0,{addr:x},4")

    print(f"\n================================================================", flush=True)
    print(f"=== Phase 8.2b Complete ===", flush=True)
    print(f"  Final vCPU0 PC = 0x{get_reg(s, 32, 'p01.01'):016x}", flush=True)
    print(f"  Final vCPU1 PC = 0x{get_reg(s, 32, 'p01.02'):016x}", flush=True)
    print(f"================================================================\n", flush=True)

finally:
    s.close()
    proc.terminate()
