"""
Phase 8.2 (Corrected): Callback A/B Experiment

FIXES from run_phase82_identify_fn.py:
1. Single-step past breakpoint after setting X0, so add executes
2. First call returns 0 -> X26=0, X23=0xc8 -> callback never called again
   So we must force X0=5001 (>= 5000 threshold) on the FIRST callback only,
   then check if the outer loop exits.

Callback is only invoked at 0xFFFFFF80080DC230 when X26 >= X23 (current target).
After first call with X0=0: X26=0, X23=0xc8. Since 0 < 0xc8, callback is skipped.
We need to force X0=5001 on first call -> X26=5001 >= 5000 -> loop exits.
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
    "-gdb", "tcp::1324,server,nowait", "-S", "-d", "guest_errors"
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
    """Write register via GDB RSP P command"""
    send_rsp(s, f"Hg{thread}")
    val_hex = struct.pack('<Q', val & 0xFFFFFFFFFFFFFFFF).hex()
    return send_rsp(s, f"P{r:x}={val_hex}")

md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10.0)
    s.connect(("127.0.0.1", 1324))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    print("\n================================================================", flush=True)
    print("=== Phase 8.2 Corrected A/B Experiment ===", flush=True)
    print("================================================================\n", flush=True)

    # Fast-forward to the blr x8 at 0xFFFFFF80080DC230
    bp_blr  = 0xFFFFFF80080DC230   # blr x8 — callback dispatch
    bp_add  = 0xFFFFFF80080DC234   # add x26, x0, x26 — accumulator update
    bp_exit = 0xFFFFFF80080DC370   # next initcall entry — success indicator

    send_rsp(s, f"Z0,{bp_blr:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.5)
    send_rsp(s, f"z0,{bp_blr:x},4")

    pc = get_reg(s, 32)
    x26 = get_reg(s, 26)
    x23 = get_reg(s, 23)
    x8  = get_reg(s, 8)

    print(f"Reached blr at PC=0x{pc:016x}", flush=True)
    print(f"  X26 (accumulator) = {x26}", flush=True)
    print(f"  X23 (target)      = {x23}", flush=True)
    print(f"  X8  (fn pointer)  = 0x{x8:016x}", flush=True)

    # ================================================================
    # CORRECTED A/B EXPERIMENT:
    # We are AT the blr instruction (PC = 0xFFFFFF80080DC230).
    # The blr hasn't executed yet. We want to:
    #   1. Let the blr execute normally (vCont;s) to jump into callback
    #   2. Let callback execute and return (continue to 0x234)
    #   3. At 0x234 (BEFORE add executes): set X0 = 5001
    #   4. Remove breakpoint, single-step (execute the add)
    #   5. Set breakpoint at 0x370 (next initcall) and continue
    # ================================================================

    print(f"\n[A/B Experiment: Forcing X0=5001 on first callback return]", flush=True)
    print(f"  Logic: timeout_val=5 * 1000 = 5000. X0=5001 will make X26=5001 >= 5000.", flush=True)

    # Step 1: Let blr execute (single step past the blr)
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.05)
    pc_after_blr = get_reg(s, 32)
    print(f"  After single-step blr: PC=0x{pc_after_blr:016x}", flush=True)

    # Step 2: Run until the callback returns (breakpoint at 0x234)
    send_rsp(s, f"Z0,{bp_add:x},4")
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(0.5)
    send_rsp(s, f"z0,{bp_add:x},4")   # remove immediately so we can step

    pc_ret = get_reg(s, 32)
    x0_orig = get_reg(s, 0)
    print(f"  Callback returned: PC=0x{pc_ret:016x} | Original X0={x0_orig}", flush=True)

    if pc_ret != bp_add:
        print(f"  ERROR: Did not reach 0x{bp_add:x}. Unexpected stop.", flush=True)
    else:
        # Step 3: Override X0 = 5001 BEFORE the add executes
        set_reg(s, 0, 5001)
        x0_check = get_reg(s, 0)
        print(f"  Forced X0 = {x0_check} (was {x0_orig})", flush=True)

        # Step 4: Single-step to execute add x26, x0, x26
        send_rsp(s, "vCont;s:p01.01")
        time.sleep(0.05)
        pc_after_add = get_reg(s, 32)
        x26_new = get_reg(s, 26)
        x23_new = get_reg(s, 23)
        print(f"  After add executes: PC=0x{pc_after_add:016x} | X26={x26_new} | X23={x23_new}", flush=True)

        # Step 5: Single-step through add x23, x26, #0xc8
        send_rsp(s, "vCont;s:p01.01")
        time.sleep(0.05)
        x23_updated = get_reg(s, 23)
        print(f"  After X23 update: X23={x23_updated}", flush=True)

        # Step 6: Continue to the outer loop exit check (0xFFFFFF80080DC268)
        # or next initcall (0xFFFFFF80080DC370)
        outer_check = 0xFFFFFF80080DC264   # cmp x26, w8, sxtw
        send_rsp(s, f"Z0,{outer_check:x},4")
        send_rsp(s, f"Z0,{bp_exit:x},4")
        send_rsp(s, "vCont;c:p01.01")
        time.sleep(1.0)
        send_rsp(s, f"z0,{outer_check:x},4")
        send_rsp(s, f"z0,{bp_exit:x},4")

        pc_final = get_reg(s, 32)
        x26_final = get_reg(s, 26)
        x8_final  = get_reg(s, 8)

        print(f"\n  === Result ===", flush=True)
        if pc_final == bp_exit:
            print(f"  *** LOOP EXITED: Reached next initcall dispatch @ 0x{pc_final:016x} ***", flush=True)
            print(f"  X26 final = {x26_final}", flush=True)
            print(f"  CONCLUSION: The callback return value IS the blocking variable.", flush=True)
        elif pc_final == outer_check:
            # Step the comparison to see which branch is taken
            print(f"  Reached outer loop exit check @ 0x{pc_final:016x}", flush=True)
            send_rsp(s, "vCont;s:p01.01")
            time.sleep(0.05)
            pc_branch = get_reg(s, 32)
            print(f"  After cmp, branch taken to: 0x{pc_branch:016x}", flush=True)
            if pc_branch == 0xFFFFFF80080DC20C:
                print(f"  Loop branched BACK (b.lt taken) -> X26={x26_final} < threshold", flush=True)
            else:
                print(f"  Loop EXITED (b.lt not taken) -> X26={x26_final} >= threshold", flush=True)
        else:
            print(f"  Stopped at unexpected PC=0x{pc_final:016x} | X26={x26_final} | X8=0x{x8_final:016x}", flush=True)

    # ================================================================
    # Final state of both CPUs
    # ================================================================
    print(f"\n================================================================", flush=True)
    print(f"=== Phase 8.2 Final CPU State ===", flush=True)
    print(f"  vCPU0 PC = 0x{get_reg(s, 32, 'p01.01'):016x}", flush=True)
    print(f"  vCPU1 PC = 0x{get_reg(s, 32, 'p01.02'):016x}", flush=True)
    print(f"================================================================\n", flush=True)

finally:
    s.close()
    proc.terminate()
