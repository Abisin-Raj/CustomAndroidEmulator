"""
Phase 8.2c: Trace the tight loop around 0xFFFFFF80080DC27C.
We need to know if:
  A) The kernel is cycling through do_initcalls quickly (good - means initcalls complete fast)
  B) The kernel is stuck in the same delay calibration loop again (means the same bug repeats for every initcall)
  C) Something else entirely

Strategy: Force X26=5001 to exit the calibration loop, then set breakpoints at
ALL key addresses in the 0xDC000-0xDC400 range to see every PC the kernel visits
in sequence over the next few hundred steps.
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
    "-gdb", "tcp::1328,server,nowait", "-S", "-d", "guest_errors"
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
    s.connect(("127.0.0.1", 1328))
    s.sendall(b"+")

    print("\n================================================================", flush=True)
    print("=== Phase 8.2c: Tight Loop Trace at 0xFFFFFF80080DC27C ===", flush=True)
    print("================================================================\n", flush=True)

    # Fast-forward to first blr x8 and apply X26 override
    bp_blr = 0xFFFFFF80080DC230
    bp_add = 0xFFFFFF80080DC234

    send_rsp(s, f"Z0,{bp_blr:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.5)
    send_rsp(s, f"z0,{bp_blr:x},4")

    # Single-step blr, then let callback return
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.05)

    send_rsp(s, f"Z0,{bp_add:x},4")
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(0.5)
    send_rsp(s, f"z0,{bp_add:x},4")

    # Force X0=5001, single-step the add and x23 update
    set_reg(s, 0, 5001)
    send_rsp(s, "vCont;s:p01.01"); time.sleep(0.05)  # add x26, x0, x26
    send_rsp(s, "vCont;s:p01.01"); time.sleep(0.05)  # add x23, x26, #0xc8

    x26 = get_reg(s, 26)
    print(f"X26 forced to {x26} — continuing to loop exit...", flush=True)

    # Let the udelay loop and outer check execute freely until we reach 0xFFFFFF80080DC27C
    bp_taint = 0xFFFFFF80080DC27C
    send_rsp(s, f"Z0,{bp_taint:x},4")
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(1.0)
    send_rsp(s, f"z0,{bp_taint:x},4")

    pc = get_reg(s, 32)
    print(f"Reached: PC=0x{pc:016x}\n", flush=True)

    # Now do a 50-step single-instruction trace to observe the tight loop
    print("[50-step single-instruction trace from 0xFFFFFF80080DC27C]", flush=True)

    prev_pc = pc
    for step in range(80):
        mem4 = get_mem(s, prev_pc, 4)
        d = list(md.disasm(mem4, prev_pc)) if len(mem4) == 4 else []
        insn_str = f"{d[0].mnemonic}\t{d[0].op_str}" if d else "???"
        
        x26 = get_reg(s, 26)
        x0  = get_reg(s, 0)
        
        print(f"  [{step:02d}] 0x{prev_pc:016x}: {insn_str:<40} | X0=0x{x0:x} X26={x26}", flush=True)
        
        send_rsp(s, "vCont;s:p01.01")
        time.sleep(0.01)
        prev_pc = get_reg(s, 32)

        # Stop early if we've hit the next initcall dispatch
        if prev_pc == 0xFFFFFF80080DC370:
            print(f"  [{step+1:02d}] *** Reached next initcall dispatch @ 0x{prev_pc:016x} ***", flush=True)
            break
        if prev_pc == 0xFFFFFF80080DC008:
            print(f"  [{step+1:02d}] *** Back to do_initcalls loop top @ 0x{prev_pc:016x} ***", flush=True)
            break

    print(f"\nFinal PC: 0x{prev_pc:016x}", flush=True)

finally:
    s.close()
    proc.terminate()
