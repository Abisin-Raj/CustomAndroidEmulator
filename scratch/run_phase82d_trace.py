"""
Phase 8.2d: Continue the instruction trace from 0xFFFFFF80080BA75C
to see what happens next — specifically whether we return back to
0xFFFFFF80080DC27C or proceed to the next initcall.
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
    "-gdb", "tcp::1330,server,nowait", "-S", "-d", "guest_errors"
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

def get_mem(s, addr, size, thread="p01.01"):
    send_rsp(s, f"Hg{thread}")
    resp = send_rsp(s, f"m{addr:x},{size}")
    for part in resp.split('$'):
        if part and '#' in part:
            raw = part.split('#')[0]
            if len(raw) >= size * 2:
                try: return bytes.fromhex(raw[:size*2])
                except Exception: pass
    return b""

md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10.0)
    s.connect(("127.0.0.1", 1330))
    s.sendall(b"+")

    print("\n================================================================", flush=True)
    print("=== Phase 8.2d: Trace from 0xFFFFFF80080BA75C ===", flush=True)
    print("================================================================\n", flush=True)

    # Fast-forward to blr x8 and apply X26 override (same as before)
    bp_blr = 0xFFFFFF80080DC230
    bp_add = 0xFFFFFF80080DC234

    send_rsp(s, f"Z0,{bp_blr:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.5)
    send_rsp(s, f"z0,{bp_blr:x},4")

    send_rsp(s, "vCont;s:p01.01"); time.sleep(0.05)  # step blr

    send_rsp(s, f"Z0,{bp_add:x},4")
    send_rsp(s, "vCont;c:p01.01"); time.sleep(0.5)
    send_rsp(s, f"z0,{bp_add:x},4")

    set_reg(s, 0, 5001)
    send_rsp(s, "vCont;s:p01.01"); time.sleep(0.05)
    send_rsp(s, "vCont;s:p01.01"); time.sleep(0.05)

    # Continue to 0xFFFFFF80080DC27C
    bp_taint = 0xFFFFFF80080DC27C
    send_rsp(s, f"Z0,{bp_taint:x},4")
    send_rsp(s, "vCont;c:p01.01"); time.sleep(1.0)
    send_rsp(s, f"z0,{bp_taint:x},4")

    pc = get_reg(s, 32)
    print(f"Starting at: PC=0x{pc:016x}", flush=True)

    # Step 80 instructions through the taint check + function calls
    for _ in range(80):
        send_rsp(s, "vCont;s:p01.01")
        time.sleep(0.01)

    prev_pc = get_reg(s, 32)
    print(f"After 80 steps, at 0x{prev_pc:016x}. Continuing with key breakpoints...\n", flush=True)

    # Now set a comprehensive set of breakpoints to observe what follows
    milestones = {
        0xFFFFFF80080DC27C: "taint check (delay loop start)",
        0xFFFFFF80080DC200: "delay calibration outer loop top",
        0xFFFFFF80080DC230: "blr x8 callback",
        0xFFFFFF80080DC368: "bl 0x811BD60 (after taint check)",
        0xFFFFFF80080DC370: "next initcall dispatch",
        0xFFFFFF80080DC374: "initcall return",
        0xFFFFFF80080DC008: "do_initcalls loop top",
        0xFFFFFF80080A93C0: "function 0x80A93C0",
        0xFFFFFF800811BD60: "function 0x811BD60",
    }

    for addr in milestones:
        send_rsp(s, f"Z0,{addr:x},4")

    print("[Continuing — watching for sequence of milestone hits...]", flush=True)
    visit_log = []
    for i in range(20):
        send_rsp(s, "vCont;c:p01.01")
        time.sleep(0.05)
        hit_pc = get_reg(s, 32)
        label = milestones.get(hit_pc, f"0x{hit_pc:016x}")
        visit_log.append((hit_pc, label))
        print(f"  [{i:02d}] PC=0x{hit_pc:016x} : {label}", flush=True)

        # If we hit the callback loop again, note it and continue
        if hit_pc == 0xFFFFFF80080DC230:
            print(f"       (callback loop re-entered — same stall pattern)", flush=True)
        if hit_pc == 0xFFFFFF80080DC370:
            x19 = get_reg(s, 19)
            print(f"       (next initcall function = 0x{x19:016x})", flush=True)

    for addr in milestones:
        send_rsp(s, f"z0,{addr:x},4")

    # Summarize
    print(f"\n[Summary — distinct PCs visited in order]", flush=True)
    seen = []
    for pc, label in visit_log:
        if not seen or seen[-1][0] != pc:
            seen.append((pc, label))
    for pc, label in seen:
        print(f"  0x{pc:016x} : {label}", flush=True)

    print(f"\n================================================================", flush=True)
    print(f"=== Phase 8.2d Complete ===", flush=True)
    print(f"  Final vCPU0 PC = 0x{get_reg(s, 32, 'p01.01'):016x}", flush=True)
    print(f"================================================================\n", flush=True)

finally:
    s.close()
    proc.terminate()
