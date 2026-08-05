import socket
import time
import subprocess
import os
import struct
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM

qemu_bin = r"C:\qemu_source\build\qemu-system-aarch64.exe"
kernel_img = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\kernel.img"
sym_map_file = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\System.map.stub"

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
    "-gdb", "tcp::1320,server,nowait",
    "-S",
    "-d", "guest_errors"
]

proc = subprocess.Popen(cmd, env=my_env)
time.sleep(1.5)

def send_rsp(s, payload):
    checksum = sum(payload.encode('utf-8')) % 256
    pkt = f"${payload}#{checksum:02x}"
    s.sendall(pkt.encode('utf-8'))
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
            clean_hex = part.split('#')[0]
            try: out += bytes.fromhex(clean_hex).decode(errors='ignore')
            except Exception: pass
    return out

def get_reg(s, reg_num, thread="p01.01"):
    send_rsp(s, f"Hg{thread}")
    resp = send_rsp(s, f"p{reg_num:x}")
    for part in resp.split('$'):
        if part and '#' in part:
            raw = part.split('#')[0]
            if len(raw) >= 16:
                try:
                    return struct.unpack('<Q', bytes.fromhex(raw[:16]))[0]
                except Exception: pass
    return 0

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

def disasm_block(s, addr, size, current_pc=None):
    mem = get_mem(s, addr, size)
    for off in range(0, len(mem), 4):
        a = addr + off
        d = list(md.disasm(mem[off:off+4], a))
        if d:
            marker = " <=== PC" if a == current_pc else ""
            print(f"  0x{a:016x}: {d[0].mnemonic}\t{d[0].op_str}{marker}", flush=True)

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect(("127.0.0.1", 1320))
    s.sendall(b"+")

    # Fast-forward to the blr x8 dispatch at 0xFFFFFF80080DC230
    bp = 0xFFFFFF80080DC230
    send_rsp(s, f"Z0,{bp:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.0)
    send_rsp(s, f"z0,{bp:x},4")

    # ====================================================================
    # Q1: Read all context registers to understand the full loop state
    # ====================================================================
    x21 = get_reg(s, 21)   # base of clock ops struct
    x22 = get_reg(s, 22)   # reference pointer (ops table)
    x23 = get_reg(s, 23)   # timeout target (X26 + 0xc8)
    x24 = get_reg(s, 24)   # descriptor table base
    x25 = get_reg(s, 25)   # multiplier (1000)
    x26 = get_reg(s, 26)   # accumulator (sum of ops returns)
    x27 = get_reg(s, 27)   # timing constant
    x28 = get_reg(s, 28)   # per-CPU data pointer

    print("\n============================================================", flush=True)
    print("=== Phase 8.1: Hardware Polling Register Identification ===", flush=True)
    print("============================================================\n", flush=True)

    print("[Context Registers at blr x8]", flush=True)
    print(f"  X21 (clock ops struct base) = 0x{x21:016x}", flush=True)
    print(f"  X22 (ops table reference)   = 0x{x22:016x}", flush=True)
    print(f"  X23 (timeout target)        = 0x{x23:016x}", flush=True)
    print(f"  X24 (descriptor table)      = 0x{x24:016x}", flush=True)
    print(f"  X25 (multiplier = 1000)     = 0x{x25:016x}", flush=True)
    print(f"  X26 (accumulator)           = 0x{x26:016x}", flush=True)
    print(f"  X27 (timing constant)       = 0x{x27:016x}", flush=True)
    print(f"  X28 (per-CPU data ptr)      = 0x{x28:016x}", flush=True)

    # ====================================================================
    # Q2: Read the timeout register value at [x24, #0xaa4] in RAM
    # ====================================================================
    timeout_val_bytes = get_mem(s, x24 + 0xaa4, 4)
    timeout_val = struct.unpack('<I', timeout_val_bytes)[0] if len(timeout_val_bytes) == 4 else 0
    print(f"\n[Timeout Register: [X24+0xAA4] = [0x{x24+0xaa4:016x}]]", flush=True)
    print(f"  Current value = 0x{timeout_val:08x} ({timeout_val})", flush=True)
    print(f"  Effective timeout = timeout_val * X25(1000) = {timeout_val * 0x3e8}", flush=True)

    # Translate physical address of the timeout register
    gpa_resp = send_hmp(s, f"gva2gpa 0x{x24+0xaa4:x}")
    print(f"  Physical address: {gpa_resp.strip()}", flush=True)

    # ====================================================================
    # Q3: Read the ops table and identify what type of clock this is
    # ====================================================================
    print(f"\n[Clock Ops Struct @ X21=0x{x21:016x}]", flush=True)

    # Read ops table pointer at [X21, #0xbe0]
    ops_tbl_ptr_bytes = get_mem(s, x21 + 0xbe0, 8)
    ops_tbl_ptr = struct.unpack('<Q', ops_tbl_ptr_bytes)[0] if len(ops_tbl_ptr_bytes) == 8 else 0
    print(f"  [X21+0xBE0] ops table ptr = 0x{ops_tbl_ptr:016x}", flush=True)

    # Read ops pointer 0 and 1 (index into dispatch table at 0xFFFFFF8008080230)
    op0_bytes = get_mem(s, 0xFFFFFF8008080230, 8)
    op0 = struct.unpack('<Q', op0_bytes)[0] if len(op0_bytes) == 8 else 0
    print(f"  Dispatch table[0] @ 0xFFFFFF8008080230 = 0x{op0:016x}", flush=True)

    # Read string at X21+0xbe8 (the "Not tainted" or clock name string)
    name_bytes = get_mem(s, x21 + 0xbe8, 64)
    clock_name = name_bytes.split(b'\x00')[0].decode(errors='replace')
    print(f"  [X21+0xBE8] clock name/string = \"{clock_name}\"", flush=True)

    # Read string at X21+0x7d9 (init message seen in disassembly)
    init_str_bytes = get_mem(s, x21 + 0x7d9, 64)
    init_str = init_str_bytes.split(b'\x00')[0].decode(errors='replace')
    print(f"  [X21+0x7D9] init string = \"{init_str}\"", flush=True)

    # ====================================================================
    # Q4: What does the blr x8 target function actually do?
    # ====================================================================
    print(f"\n[Ops Callback: blr X8 -> 0xFFFFFF8008080238 -> 0xFFFFFF80080DC4C0]", flush=True)
    print(f"  (returns X0=0 unconditionally)", flush=True)

    # Step into the function to get full picture: what is the "real" read supposed to return?
    # Step through one full outer loop iteration and read what changes
    print(f"\n[Stepping through one full outer loop iteration...]", flush=True)

    x26_before = get_reg(s, 26)

    # Set breakpoint at 0xFFFFFF80080DC234 (instruction after blr x8)
    send_rsp(s, "Z0,ffffff80080dc234,4")
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(0.1)
    send_rsp(s, "z0,ffffff80080dc234,4")

    x0_ret = get_reg(s, 0)    # what blr x8 returned
    x26_after = get_reg(s, 26)

    print(f"  blr x8 returned X0 = 0x{x0_ret:016x}", flush=True)
    print(f"  X26 (accumulator) before = 0x{x26_before:016x}", flush=True)
    print(f"  X26 (accumulator) after  = 0x{x26_after:016x}  (delta = {x0_ret})", flush=True)

    # ====================================================================
    # Summary
    # ====================================================================
    print(f"\n============================================================", flush=True)
    print(f"=== Phase 8.1 BET 4-Question Register Identification ===", flush=True)
    print(f"  Q1. Register being read: [X24+0xAA4] = [0x{x24+0xaa4:016x}]", flush=True)
    gpa_clean = gpa_resp.strip()
    print(f"      Physical address: {gpa_clean} (kernel RAM, not MMIO)", flush=True)
    print(f"  Q2. Expected value:   timeout_val > 0 (current = {timeout_val})", flush=True)
    print(f"  Q3. Owning device:    Clock ops struct at X21=0x{x21:016x}", flush=True)
    print(f"      String: \"{clock_name}\" / \"{init_str}\"", flush=True)
    print(f"  Q4. Current read val: ops callback always returns X0=0", flush=True)
    print(f"      -> accumulator X26 never advances -> outer loop never exits", flush=True)
    print(f"============================================================\n", flush=True)

finally:
    s.close()
    proc.terminate()
