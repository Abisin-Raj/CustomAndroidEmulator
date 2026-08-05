import socket
import time
import subprocess
import sys
import struct

qemu_bin = r"C:\qemu_source\build\qemu-system-aarch64.exe"


kernel_img = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\kernel.img"

cmd = [
    qemu_bin,
    "-machine", "virt",
    "-cpu", "cortex-a76,psci-conduit=1",
    "-smp", "2",
    "-m", "4096",
    "-nographic",
    "-kernel", kernel_img,
    "-append", "console=ttyAMA0 earlycon=pl011,0x09000000 androidboot.hardware=qcom nokaslr",
    "-gdb", "tcp::1240,server,nowait",
    "-S",
    "-d", "guest_errors"
]

print("[Phase 7.3] Starting QEMU with Real PixelExperience Kernel Image on port 1240...", flush=True)
proc = subprocess.Popen(cmd)
time.sleep(1.5)
print(f"[QEMU Poll Status]: {proc.poll()}", flush=True)


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

def get_reg(s, reg_num, thread="p01.02"):
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

def get_mem(s, addr, size, thread="p01.02"):
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

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(4.0)
    s.connect(("127.0.0.1", 1240))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    # 1. Inspect vCPU0 initial PC
    cpu0_pc = get_reg(s, 32, "p01.01") # Reg 32 = PC
    print(f"[Phase 7.3] vCPU0 Initial PC: 0x{cpu0_pc:016x}", flush=True)

    # 2. Wake CPU1 at 0x40080000 (kernel entry in RAM)
    entry_addr = 0x40080000
    res = send_hmp(s, f"compat-wake-cpu 1 0x{entry_addr:x} 0")
    print(f"[Phase 7.3 HMP]: {res.strip()}", flush=True)

    print("\n--- Phase 7.3: Secondary CPU Early Initialization Progress ---", flush=True)
    
    current_block = ""
    block_insn_count = 0
    execution_blocks = []
    
    cache_loop_exited = False
    mmu_enabled_detected = False
    secondary_start_kernel_reached = False
    
    stop_reason = "Max steps reached"
    last_pc = 0
    last_x0 = 0
    last_x1 = 0
    last_x2 = 0
    
    # Trace up to 5000 steps to observe 2nd cache loop completion, MMU enablement, and virtual space jump
    for step in range(1, 5001):





        pc = get_reg(s, 32, "p01.02")   # Reg 32 = PC
        x0 = get_reg(s, 0, "p01.02")    # Reg 0 = X0
        x1 = get_reg(s, 1, "p01.02")    # Reg 1 = X1
        x2 = get_reg(s, 2, "p01.02")    # Reg 2 = X2

        mem_bytes = get_mem(s, pc, 4, "p01.02") if pc != 0 else b""
        insn_val = struct.unpack('<I', mem_bytes)[0] if len(mem_bytes) == 4 else 0

        # Classify execution block
        block_name = "Boot Stub"
        if pc >= 0x400a0738 and pc <= 0x400a0748:
            block_name = "invalidate_icache_range (Cache Loop)"
        elif pc >= 0xFFFFFF8000000000:
            block_name = "Virtual Memory Space (Higher-Half)"
            if pc >= 0xFFFFFF8008090200:
                block_name = "secondary_start_kernel()"
                secondary_start_kernel_reached = True
            elif pc >= 0xFFFFFF8008090100:
                block_name = "__secondary_switched"

        if block_name != current_block:
            if current_block != "":
                execution_blocks.append((current_block, block_insn_count))
                print(f"[BET Block] {current_block} -> Executed {block_insn_count} instructions", flush=True)
            current_block = block_name
            block_insn_count = 0

        block_insn_count += 1

        if cache_loop_exited and block_name == "Boot Stub":
            if step % 20 == 0 or step < 1370:
                lr = get_reg(s, 30, "p01.02")
                sp = get_reg(s, 31, "p01.02")
                x3 = get_reg(s, 3, "p01.02")
                x4 = get_reg(s, 4, "p01.02")
                print(f"  [Boot Stub Step {step:04d}] PC=0x{pc:016x} | Opcode=0x{insn_val:08x} | X0=0x{x0:x} X1=0x{x1:x} X2=0x{x2:x} X3=0x{x3:x} X4=0x{x4:x} SP=0x{sp:x} LR=0x{lr:x}", flush=True)


        # Step 1 Progress Tracking: X0 vs X1 in cache loop
        if block_name == "invalidate_icache_range (Cache Loop)":
            if step % 10 == 0 or step == 1:
                print(f"  [Cache Loop Step {step:03d}] PC=0x{pc:016x} | X0=0x{x0:016x} -> X1=0x{x1:016x} (Stride X2=0x{x2:x})", flush=True)

        if last_pc in [0x400a0744, 0x400a0748] and pc not in [0x400a0738, 0x400a073c, 0x400a0740, 0x400a0744, 0x400a0748]:
            if not cache_loop_exited:
                cache_loop_exited = True
                print(f"\n[Phase 7.3 Milestone] Cache loop EXITED cleanly at Step {step}! X0 reached 0x{x0:016x} (X1=0x{x1:016x})", flush=True)

        if pc >= 0xFFFFFF8000000000 and not mmu_enabled_detected:
            mmu_enabled_detected = True
            print(f"\n[Phase 7.3 Milestone] MMU Enablement & Virtual Space Jump DETECTED at Step {step}! PC=0x{pc:016x}", flush=True)

        if pc == 0x200 or (pc & 0xFFF) == 0x200:
            stop_reason = f"Trapped to ARM64 Exception Vector (0x200) from previous PC 0x{last_pc:016x}"
            print(f"\n[Phase 7.3 Exception Trap Detected!]", flush=True)
            print(f"  Previous PC before trap: 0x{last_pc:016x}", flush=True)
            prev_mem = get_mem(s, last_pc - 16, 32, "p01.02")
            if len(prev_mem) == 32:
                for off in range(0, 32, 4):
                    addr_chunk = (last_pc - 16) + off
                    code_val = struct.unpack('<I', prev_mem[off:off+4])[0]
                    marker = " <== FAULTING INSTRUCTION" if addr_chunk == last_pc else ""
                    print(f"    0x{addr_chunk:016x}: 0x{code_val:08x}{marker}", flush=True)
            hmp_info = send_hmp(s, "info registers")
            print(f"\n  [CPU1 Register State at Exception]:\n{hmp_info.strip()}", flush=True)
            break


        if secondary_start_kernel_reached:
            stop_reason = "Reached secondary_start_kernel()!"
            break

        last_pc = pc
        last_x0 = x0
        last_x1 = x1
        last_x2 = x2

        # Step vCPU1
        send_rsp(s, "vCont;s:p01.02")

    if block_insn_count > 0:
        execution_blocks.append((current_block, block_insn_count))
        print(f"[BET Block] {current_block} -> Executed {block_insn_count} instructions", flush=True)

    print("\n============================================================================", flush=True)
    print("=== Phase 7.3 Execution Block & Progress Summary ===", flush=True)
    print(f"  Final PC:                          0x{last_pc:016x}", flush=True)
    print(f"  Cache Loop Exited Cleanly:        {'YES' if cache_loop_exited else 'NO (Looping or In Progress)'}", flush=True)
    print(f"  MMU Enabled (Virtual Jump):        {'YES' if mmu_enabled_detected else 'NO'}", flush=True)
    print(f"  secondary_start_kernel Reached:   {'YES' if secondary_start_kernel_reached else 'NO'}", flush=True)
    print(f"  Stop Reason:                       {stop_reason}", flush=True)
    print("\n  High-Level Execution Blocks:", flush=True)
    for b_name, b_count in execution_blocks:
        print(f"    - {b_name}: {b_count} instructions", flush=True)
    print("============================================================================", flush=True)

    time.sleep(0.5)
    s.close()
finally:
    proc.terminate()
    print("\n[Test Complete]", flush=True)
