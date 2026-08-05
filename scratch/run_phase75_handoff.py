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
    "-gdb", "tcp::1258,server,nowait",
    "-S",
    "-d", "guest_errors"
]

print("[Phase 7.5] Starting QEMU on port 1258 for Accelerated Secondary Kernel Handoff...", flush=True)
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

md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect(("127.0.0.1", 1258))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    # Wake CPU1 at 0x40080000
    entry_addr = 0x40080000
    res = send_hmp(s, f"compat-wake-cpu 1 0x{entry_addr:x} 0")
    print(f"[Phase 7.5 HMP]: {res.strip()}", flush=True)

    print("\n--- Phase 7.5: Secondary Kernel Handoff (BSS Breakpoint Accelerated) ---", flush=True)

    first_milestones = {
        "First MMU Enable": None,
        "First Virtual Instruction": None,
        "First __secondary_switched": None,
        "First BSS Zeroing Complete": None,
        "First secondary_start_kernel": None,
        "First CPU Online Signal": None,
        "First Idle Loop Entry": None
    }

    current_block = ""
    block_insn_count = 0
    function_metrics = []

    terminal_state = "Max steps reached"
    terminal_details = ""

    # Set GDB Breakpoint at BSS loop exit (0xFFFFFF80080A6CBC) and secondary_start_kernel (0xFFFFFF800A400860)
    send_rsp(s, "Z0,ffffff80080a6cbc,4")
    send_rsp(s, "Z0,ffffff800a400860,4")
    print("[Phase 7.5] Set GDB Breakpoints at BSS Exit (0xFFFFFF80080A6CBC) and secondary_start_kernel (0xFFFFFF800A400860)", flush=True)


    # Step once to kick vCPU1 into TCG execution
    send_rsp(s, "vCont;s:p01.02")
    time.sleep(0.1)

    last_pc = 0

    # Step up to 25,000 steps to trace through secondary_start_kernel and SMP online handshake
    for step in range(1, 25001):

        pc = get_reg(s, 32, "p01.02")
        x0 = get_reg(s, 0, "p01.02")
        x1 = get_reg(s, 1, "p01.02")
        x2 = get_reg(s, 2, "p01.02")
        x3 = get_reg(s, 3, "p01.02")
        lr = get_reg(s, 30, "p01.02")
        sp = get_reg(s, 31, "p01.02")
        pstate = get_reg(s, 33, "p01.02")

        mem_bytes = get_mem(s, pc, 4, "p01.02") if pc != 0 else b""
        insn_val = struct.unpack('<I', mem_bytes)[0] if len(mem_bytes) == 4 else 0

        disasm_str = ""
        mnemonic = ""
        if insn_val != 0:
            disasm_list = list(md.disasm(mem_bytes, pc))
            if disasm_list:
                mnemonic = disasm_list[0].mnemonic
                disasm_str = f"{mnemonic}\t{disasm_list[0].op_str}"

        # First-Occurrence Milestones
        if pc == 0x41c9c358 and first_milestones["First MMU Enable"] is None:
            first_milestones["First MMU Enable"] = (step, pc)
        if pc >= 0xFFFFFF8000000000 and first_milestones["First Virtual Instruction"] is None:
            first_milestones["First Virtual Instruction"] = (step, pc)
        if pc >= 0xFFFFFF800A400190 and pc <= 0xFFFFFF800A400200 and first_milestones["First __secondary_switched"] is None:
            first_milestones["First __secondary_switched"] = (step, pc)
        if pc == 0xFFFFFF80080A6CBC and first_milestones["First BSS Zeroing Complete"] is None:
            first_milestones["First BSS Zeroing Complete"] = (step, pc)
            print(f"\n[Phase 7.5 Milestone] BSS Zeroing Loop COMPLETED! Exited to 0x{pc:016x}", flush=True)
        if (pc == 0xFFFFFF800A400860 or (pc >= 0xFFFFFF8008090200 and pc <= 0xFFFFFF8008090300)) and first_milestones["First secondary_start_kernel"] is None:
            first_milestones["First secondary_start_kernel"] = (step, pc)
            print(f"\n[Phase 7.5 Milestone] secondary_start_kernel() REACHED at PC=0x{pc:016x}! (Step {step})", flush=True)

        if mnemonic == "wfi" and first_milestones["First Idle Loop Entry"] is None:
            first_milestones["First Idle Loop Entry"] = (step, pc)

        # Classify execution block
        block_name = "Boot Stub (Physical RAM)"
        if pc >= 0x400a0738 and pc <= 0x400a0748:
            block_name = "invalidate_icache_range (Cache Maintenance)"
        elif pc >= 0xFFFFFF8000000000:
            block_name = "__secondary_switched (Virtual Space)"
            if pc >= 0xFFFFFF80080a6cac and pc <= 0xFFFFFF80080a6cb8:
                block_name = "__secondary_switched BSS Zeroing (dc zva)"
            elif pc >= 0xFFFFFF8008090200 and pc <= 0xFFFFFF8008090400:
                block_name = "secondary_start_kernel()"

        if block_name != current_block:
            if current_block != "":
                function_metrics.append((current_block, block_insn_count))
                print(f"\n[BET Block Complete] {current_block} -> Executed {block_insn_count} instructions", flush=True)
            
            current_block = block_name
            block_insn_count = 0

            # Capture Full Register Snapshot at Milestone Block Entry
            print(f"\n----------------------------------------------------------------------------", flush=True)
            print(f"[Milestone Entry: {block_name}] @ Step {step} | PC=0x{pc:016x}", flush=True)
            print(f"  Opcode: 0x{insn_val:08x} ({disasm_str})", flush=True)
            print(f"  SP=0x{sp:016x} | LR=0x{lr:016x} | PSTATE=0x{pstate:08x}", flush=True)
            print(f"  X0=0x{x0:016x} X1=0x{x1:016x} X2=0x{x2:016x} X3=0x{x3:016x}", flush=True)
            print(f"----------------------------------------------------------------------------", flush=True)

        block_insn_count += 1

        # Fast-Forward through BSS zeroing loop via GDB continue
        if block_name == "__secondary_switched BSS Zeroing (dc zva)" and step > 4862:
            print(f"[Fast-Forward] Continuing vCPU1 to hit BSS Loop Exit Breakpoint (0xFFFFFF80080A6CBC)...", flush=True)
            send_rsp(s, "vCont;c:p01.02")
            time.sleep(0.5)

        # Check 5 Terminal Conditions
        if mnemonic == "wfi":
            terminal_state = "Condition 2: Entered Scheduler Idle Loop (wfi)"
            terminal_details = f"Reached wfi at PC=0x{pc:016x} (Step {step})"
            break

        if pc == 0x200:
            terminal_state = "Condition 4: Trapped to ARM64 Exception Vector (0x200)"
            terminal_details = f"Trapped from previous PC=0x{last_pc:016x} to 0x200 (Step {step})"
            break

        if pc < 0x40000000 or (pc > 0x43008000 and pc < 0xFFFFFF8000000000) or pc > 0xFFFFFF800FFFFFFF:
            terminal_state = "Condition 5: Execution Left Known Kernel Code Space"
            terminal_details = f"PC=0x{pc:016x} outside valid RAM / Kernel Virtual mapping (Step {step})"
            break

        last_pc = pc

        # Step vCPU1
        send_rsp(s, "vCont;s:p01.02")

    if block_insn_count > 0:
        function_metrics.append((current_block, block_insn_count))

    # Remove Breakpoint
    send_rsp(s, "z0,ffffff80080a6cbc,4")

    print("\n============================================================================", flush=True)
    print("=== Phase 7.5 Secondary CPU Milestone & Progress Summary ===", flush=True)
    print(f"  Final PC:        0x{last_pc:016x}", flush=True)
    print(f"  Terminal State:  {terminal_state}", flush=True)
    if terminal_details:
        print(f"  Details:         {terminal_details}", flush=True)

    print("\n  BET First-Occurrence Milestone Database:", flush=True)
    for m_name, m_data in first_milestones.items():
        if m_data is not None:
            m_step, m_pc = m_data
            print(f"    - {m_name:30s}: Achieved @ Step {m_step:05d} (PC=0x{m_pc:016x})", flush=True)
        else:
            print(f"    - {m_name:30s}: Pending", flush=True)

    print("\n  Instructions Executed per Functional Block:", flush=True)
    for f_name, f_count in function_metrics:
        print(f"    - {f_name:45s}: {f_count} instructions", flush=True)
    print("============================================================================", flush=True)

    time.sleep(0.5)
    s.close()
finally:
    proc.terminate()
    print("\n[Test Complete]", flush=True)
