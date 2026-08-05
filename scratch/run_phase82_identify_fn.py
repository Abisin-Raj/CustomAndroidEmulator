"""
Phase 8.2: Callback Function Identification & A/B Validation

Steps:
1. Read the full dispatch table at 0xFFFFFF8008080230 (identify all entries)
2. Trace who installed the ops pointer at [X21+0xBE0] (watchpoint during boot)
3. A/B Experiment: Force X0=1 after blr x8, see if loop exits and kernel advances
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
    "-gdb", "tcp::1322,server,nowait", "-S", "-d", "guest_errors"
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
    """Write a register value via GDB RSP P command"""
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
    s.connect(("127.0.0.1", 1322))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    # ================================================================
    # STEP 1: Fast-forward to blr x8 (0xFFFFFF80080DC230)
    # ================================================================
    bp_blr = 0xFFFFFF80080DC230
    send_rsp(s, f"Z0,{bp_blr:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.5)
    send_rsp(s, f"z0,{bp_blr:x},4")

    x21 = get_reg(s, 21)   # clock ops struct base
    x22 = get_reg(s, 22)   # dispatch table reference
    x23 = get_reg(s, 23)
    x26 = get_reg(s, 26)   # accumulator

    print("\n================================================================", flush=True)
    print("=== Phase 8.2: Callback Identification & A/B Validation ===", flush=True)
    print("================================================================\n", flush=True)

    # ================================================================
    # STEP 2: Read full dispatch table at X22 (0xFFFFFF8008080230)
    # ================================================================
    print(f"[Step 1: Dispatch Table @ X22=0x{x22:016x}]", flush=True)
    tbl_mem = get_mem(s, x22, 8 * 32)   # Read 32 jump-table entries (each 4 bytes is a branch)
    print(f"  Reading {len(tbl_mem)} bytes from dispatch table...", flush=True)

    # The table at 0xFFFFFF8008080230 contains branch instructions, each pointing to a function
    seen_targets = []
    for i in range(0, min(len(tbl_mem), 128), 4):
        addr = x22 + i
        chunk = tbl_mem[i:i+4]
        d = list(md.disasm(chunk, addr))
        if d:
            insn = d[0]
            if insn.mnemonic == 'b':
                # Extract branch target
                try:
                    target = int(insn.op_str.strip('#'), 16)
                    seen_targets.append((i // 4, addr, target))
                    print(f"  Entry[{i//4:2d}] @ 0x{addr:016x}: b 0x{target:016x}", flush=True)
                except Exception:
                    print(f"  Entry[{i//4:2d}] @ 0x{addr:016x}: {insn.mnemonic} {insn.op_str}", flush=True)
            else:
                print(f"  Entry[{i//4:2d}] @ 0x{addr:016x}: {insn.mnemonic} {insn.op_str}", flush=True)

    # ================================================================
    # STEP 3: Disassemble first 3 unique non-stub targets to identify subsystem
    # ================================================================
    print(f"\n[Step 2: Disassembly of Dispatch Table Targets (first 4 entries)]", flush=True)
    for idx, entry_addr, target in seen_targets[:4]:
        fn_mem = get_mem(s, target, 32)
        print(f"\n  Entry[{idx}] -> 0x{target:016x}:", flush=True)
        for off in range(0, len(fn_mem), 4):
            a = target + off
            d = list(md.disasm(fn_mem[off:off+4], a))
            if d:
                print(f"    0x{a:016x}: {d[0].mnemonic}\t{d[0].op_str}", flush=True)

    # ================================================================
    # STEP 4: Identify who wrote to [X21+0xBE0]
    # Read the current value at [X21+0xBE0] and nearby structure fields
    # ================================================================
    print(f"\n[Step 3: Clock Ops Struct at X21=0x{x21:016x}]", flush=True)
    # Read 256 bytes of the struct around offset 0xBD0..0xC10
    struct_mem = get_mem(s, x21 + 0xBD0, 0x50)
    for i in range(0, len(struct_mem), 8):
        val = struct.unpack('<Q', struct_mem[i:i+8])[0]
        off = 0xBD0 + i
        # Try to read as a string if looks like a kernel pointer
        str_val = ""
        if 0xFFFFFF8000000000 <= val <= 0xFFFFFFFFFFFFFFFF:
            s_bytes = get_mem(s, val, 32)
            # Only show if printable
            candidate = s_bytes.split(b'\x00')[0].decode(errors='replace')
            if all(0x20 <= ord(c) < 0x7F for c in candidate[:8]) and len(candidate) > 2:
                str_val = f'  -> "{candidate}"'
        print(f"  [X21+0x{off:03x}] = 0x{val:016x}{str_val}", flush=True)

    # ================================================================
    # STEP 5: A/B EXPERIMENT — Force X0=1 after blr x8, observe behavior
    # ================================================================
    print(f"\n[Step 4: A/B Experiment — Force X0=1 after blr x8]", flush=True)
    print(f"  Overriding X0=1 at 0xFFFFFF80080DC234 on every callback...", flush=True)

    # Set breakpoints at callback return site and at next initcall dispatch
    ret_site   = 0xFFFFFF80080DC234   # add x26, x0, x26  <- right after blr
    next_call  = 0xFFFFFF80080DC370   # bl #0xFFFFFF800819a000 <- next initcall entry

    send_rsp(s, f"Z0,{ret_site:x},4")
    send_rsp(s, f"Z0,{next_call:x},4")

    override_count = 0
    max_overrides = 6000   # plenty of room for 5000 accumulator ticks

    for i in range(max_overrides):
        send_rsp(s, "vCont;c:p01.01")
        time.sleep(0.01)

        pc = get_reg(s, 32)

        if pc == ret_site:
            # Force X0 = 1
            set_reg(s, 0, 1)
            override_count += 1
            if override_count % 500 == 0 or override_count <= 5:
                x26_now = get_reg(s, 26)
                print(f"  Override #{override_count}: X0 forced to 1. Accumulator X26={x26_now}", flush=True)

        elif pc == next_call:
            x26_final = get_reg(s, 26)
            print(f"\n  *** LOOP EXITED! Reached next initcall @ 0x{pc:016x} ***", flush=True)
            print(f"  Override count: {override_count}", flush=True)
            print(f"  Final accumulator X26 = {x26_final}", flush=True)
            break

        else:
            print(f"  Stopped at unexpected PC=0x{pc:016x} after {override_count} overrides", flush=True)
            break
    else:
        pc_final = get_reg(s, 32)
        print(f"  Max overrides ({max_overrides}) reached without loop exit. PC=0x{pc_final:016x}", flush=True)

    send_rsp(s, f"z0,{ret_site:x},4")
    send_rsp(s, f"z0,{next_call:x},4")

    # ================================================================
    # Final state
    # ================================================================
    pc0 = get_reg(s, 32, "p01.01")
    pc1 = get_reg(s, 32, "p01.02")
    print(f"\n================================================================", flush=True)
    print(f"=== Phase 8.2 Final State ===", flush=True)
    print(f"  vCPU0 PC = 0x{pc0:016x}", flush=True)
    print(f"  vCPU1 PC = 0x{pc1:016x}", flush=True)
    print(f"  Total X0=1 overrides applied: {override_count}", flush=True)
    print(f"================================================================\n", flush=True)

finally:
    s.close()
    proc.terminate()
