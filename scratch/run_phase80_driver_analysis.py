import socket
import time
import subprocess
import sys
import struct
import os
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM

qemu_bin = r"C:\qemu_source\build\qemu-system-aarch64.exe"
kernel_img = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\kernel.img"
sym_map_file = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\System.map.stub"

my_env = os.environ.copy()
my_env["PATH"] = r"C:\msys64\mingw64\bin;C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + my_env.get("PATH", "")

# Load Symbol Map
symbols = {}
if os.path.exists(sym_map_file):
    with open(sym_map_file, "r") as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 3:
                try:
                    addr = int(parts[0], 16)
                    sym_name = parts[2]
                    symbols[addr] = sym_name
                except ValueError:
                    pass

def resolve_symbol(addr):
    if addr in symbols:
        return symbols[addr]
    # Nearest symbol lookup
    best_match = None
    best_diff = 0x10000
    for s_addr, s_name in symbols.items():
        if s_addr <= addr < s_addr + 0x1000:
            diff = addr - s_addr
            if diff < best_diff:
                best_diff = diff
                best_match = f"{s_name}+0x{diff:x}" if diff > 0 else s_name
    return best_match if best_match else f"0x{addr:016x}"

cmd = [
    qemu_bin,
    "-machine", "virt",
    "-cpu", "cortex-a76,psci-conduit=1",
    "-smp", "2",
    "-m", "4096",
    "-nographic",
    "-kernel", kernel_img,
    "-append", "console=ttyAMA0 earlycon=pl011,0x09000000 androidboot.hardware=qcom nokaslr",
    "-gdb", "tcp::1292,server,nowait",
    "-S",
    "-d", "guest_errors"
]

print("[Phase 8.0] Starting QEMU on port 1292 for Function-Level Platform Driver Analysis...", flush=True)
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
    s.connect(("127.0.0.1", 1292))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    # 1. Fast forward vCPU0 to do_initcalls loop entry (0xFFFFFF80080DC370)
    initcall_entry = 0xFFFFFF80080DC370
    initcall_exit = 0xFFFFFF80080DC374

    # Set entry breakpoint BEFORE unpausing
    send_rsp(s, f"Z0,{initcall_entry:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)

    print("[Phase 8.0] Continuing vCPU0 to do_initcalls dispatch (0xFFFFFF80080DC370)...", flush=True)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(1.0)

    # Wake vCPU1
    send_hmp(s, "compat-wake-cpu 1 0x40080000 0")
    send_rsp(s, "vCont;s:p01.02")
    time.sleep(0.1)

    print("\n============================================================================", flush=True)
    print("=== Phase 8.0 Linux Boot Profiler: do_initcalls() Function Trace ===", flush=True)
    print("============================================================================\n", flush=True)

    initcall_log = []
    stalled_driver = None

    for i in range(1, 26):
        pc = get_reg(s, 32, "p01.01")
        if pc != initcall_entry:
            # Continue until hitting entry breakpoint
            send_rsp(s, "vCont;c:p01.01")
            time.sleep(0.5)
            pc = get_reg(s, 32, "p01.01")

        if pc != initcall_entry:
            print(f"  [Initcall #{i:02d}] Paused at PC=0x{pc:016x} (outside dispatch entry)", flush=True)
            stalled_driver = resolve_symbol(pc)
            break

        # Read function pointer in X19 (the driver initcall)
        fn_ptr = get_reg(s, 19, "p01.01")
        if fn_ptr == 0:
            fn_ptr = get_reg(s, 0, "p01.01")

        fn_name = resolve_symbol(fn_ptr)

        # Set exit breakpoint at 0xFFFFFF80080DC374 and continue
        send_rsp(s, f"Z0,{initcall_exit:x},4")
        send_rsp(s, "vCont;c:p01.01")
        time.sleep(0.5)

        pc_exit = get_reg(s, 32, "p01.01")
        ret_val = get_reg(s, 0, "p01.01")
        send_rsp(s, f"z0,{initcall_exit:x},4")

        if pc_exit == initcall_exit:
            print(f"  [Initcall #{i:02d}] 0x{fn_ptr:016x} ({fn_name}) -> Exited with X0=0x{ret_val:x}", flush=True)
            initcall_log.append({"idx": i, "fn_ptr": fn_ptr, "name": fn_name, "ret": ret_val, "status": "COMPLETED"})
        else:
            print(f"  [Initcall #{i:02d} STALLED/WAITING] 0x{fn_ptr:016x} ({fn_name}) did NOT return! Active PC=0x{pc_exit:016x}", flush=True)
            stalled_driver = fn_name
            initcall_log.append({"idx": i, "fn_ptr": fn_ptr, "name": fn_name, "ret": None, "status": "STALLED"})
            break


    # Remove entry breakpoint
    send_rsp(s, f"z0,{initcall_entry:x},4")

    # 3. Final State Summary Report
    cpu0_final_pc = get_reg(s, 32, "p01.01")
    cpu0_final_sp = get_reg(s, 31, "p01.01")
    cpu0_final_lr = get_reg(s, 30, "p01.01")
    cpu1_final_pc = get_reg(s, 32, "p01.02")

    print("\n============================================================================", flush=True)
    print("=== Phase 8.0 BET Platform Driver Profiling Report ===", flush=True)
    print(f"  1. Initcalls Processed         : {len(initcall_log)}", flush=True)
    print(f"  2. Active/Stalled Driver        : {stalled_driver if stalled_driver else 'All 25 Completed'}", flush=True)
    print(f"  3. vCPU0 Final Location        : PC=0x{cpu0_final_pc:016x} | SP=0x{cpu0_final_sp:016x} | LR=0x{cpu0_final_lr:016x}", flush=True)
    print(f"  4. vCPU1 Secondary Location    : PC=0x{cpu1_final_pc:016x}", flush=True)
    print("============================================================================\n", flush=True)

    if cpu0_final_pc >= 0x40000000:
        print(f"=== Disassembling Active Blocker Function around PC=0x{cpu0_final_pc:016x} ===", flush=True)
        mem_block = get_mem(s, cpu0_final_pc - 16, 0x40, "p01.01")
        if len(mem_block) == 0x40:
            for off in range(0, 0x40, 4):
                addr = (cpu0_final_pc - 16) + off
                chunk = mem_block[off:off+4]
                disasm = list(md.disasm(chunk, addr))
                if disasm:
                    insn = disasm[0]
                    marker = " <=== ACTIVE BLOCKER PC" if addr == cpu0_final_pc else ""
                    print(f"  0x{addr:016x}: {insn.mnemonic}\t{insn.op_str}{marker}", flush=True)

    time.sleep(0.5)
    s.close()
finally:
    proc.terminate()
    print("\n[Test Complete]", flush=True)
