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
    "-gdb", "tcp::1252,server,nowait",
    "-S",
    "-d", "guest_errors"
]

print("[Phase 7.4] Starting QEMU on port 1252 for Phase 7.4 Virtual Branch (br x8) Diagnostics...", flush=True)
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

def parse_hmp_sysregs(hmp_str):
    regs = {}
    for line in hmp_str.splitlines():
        line = line.strip()
        parts = line.split('=')
        if len(parts) == 2:
            key = parts[0].strip()
            val_str = parts[1].strip().split()[0]
            try:
                val = int(val_str, 16) if val_str.startswith('0x') or val_str.startswith('0X') else int(val_str, 16)
                regs[key] = val
            except Exception:
                regs[key] = val_str
    return regs

md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect(("127.0.0.1", 1252))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    # 1. Wake CPU1 at 0x40080000
    entry_addr = 0x40080000
    res = send_hmp(s, f"compat-wake-cpu 1 0x{entry_addr:x} 0")
    print(f"[Phase 7.4 HMP]: {res.strip()}", flush=True)

    print("\n--- Phase 7.4: First Virtual Branch (br x8 @ 0x41c9c378) Diagnostics ---", flush=True)
    
    recent_insns = []
    pre_branch_sysregs = {}
    x8_pre_branch = 0
    br_x8_step = 0

    # Step up to 5200 steps to reach virtual branch and trace virtual execution
    for step in range(1, 5201):

        pc = get_reg(s, 32, "p01.02")
        x8 = get_reg(s, 8, "p01.02")
        
        mem_bytes = get_mem(s, pc, 4, "p01.02") if pc != 0 else b""
        insn_val = struct.unpack('<I', mem_bytes)[0] if len(mem_bytes) == 4 else 0

        # Disassemble instruction
        disasm_str = ""
        if insn_val != 0:
            disasm_list = list(md.disasm(mem_bytes, pc))
            if disasm_list:
                disasm_str = f"{disasm_list[0].mnemonic}\t{disasm_list[0].op_str}"

        recent_insns.append((pc, insn_val, disasm_str))
        if len(recent_insns) > 30:
            recent_insns.pop(0)

        if step >= 4800:
            print(f"  [Step {step:04d}] PC=0x{pc:016x} | X8=0x{x8:016x} | {disasm_str}", flush=True)

        # 2. Capture Pre-Branch State right before br x8 (PC == 0x41c9c378)
        if pc == 0x41c9c378 or pc == 0x41c9c374:
            br_x8_step = step
            x8_pre_branch = x8
            send_hmp(s, "cpu 1")
            hmp_info = send_hmp(s, "info registers")
            pre_branch_sysregs = parse_hmp_sysregs(hmp_info)
            
            # Read literal pool entry at 0x41c9c398
            pool_bytes = get_mem(s, 0x41c9c398, 8, "p01.02")
            pool_val = struct.unpack('<Q', pool_bytes)[0] if len(pool_bytes) == 8 else 0

            print(f"\n============================================================================", flush=True)
            print(f"=== Step 1 & 2: Pre-Branch Register Snapshot @ PC=0x{pc:016x} (Step {step}) ===", flush=True)
            print(f"  Target Virtual Branch X8:              0x{x8:016x}", flush=True)
            print(f"  Literal Pool @ 0x41c9c398:             0x{pool_val:016x}", flush=True)
            print(f"  HMP System Registers (CPU 1):\n{hmp_info.strip()}", flush=True)
            print("============================================================================\n", flush=True)


        # Log instructions when executing in virtual memory (PC >= 0xFFFFFF8000000000)
        if pc >= 0xFFFFFF8000000000:
            lr = get_reg(s, 30, "p01.02")
            sp = get_reg(s, 31, "p01.02")
            print(f"  [Virtual Step {step:04d}] PC=0x{pc:016x} | Disasm: {disasm_str} | LR=0x{lr:016x} SP=0x{sp:016x}", flush=True)

        # 3. Exception Trap @ exact 0x200
        if pc == 0x200:
            print(f"\n============================================================================", flush=True)
            print(f"=== Step 3: Post-Branch Exception Trap @ PC=0x{pc:016x} (Step {step}) ===", flush=True)
            send_hmp(s, "cpu 1")
            hmp_fault = send_hmp(s, "info registers")
            print(f"  HMP Fault System Register Output:\n{hmp_fault.strip()}", flush=True)
            print("============================================================================\n", flush=True)
            
            # Perform Software Page Table Walk for X8 using TTBR1_EL1
            ttbr1_raw = pre_branch_sysregs.get("TTBR1_EL1", 0)
            if isinstance(ttbr1_raw, str):
                try:
                    ttbr1_val = int(ttbr1_raw, 16)
                except Exception:
                    ttbr1_val = 0
            else:
                ttbr1_val = ttbr1_raw

            print("\n============================================================================", flush=True)
            print(f"=== Step 4: Comprehensive 4-Level Software Page Table Walk for X8 (0x{x8_pre_branch:016x}) ===", flush=True)
            print(f"  TTBR1_EL1 Base Physical Address: 0x{ttbr1_val:016x}", flush=True)
            
            base_pa = ttbr1_val & 0x0000FFFFFFFFF000
            
            # Extract Virtual Address Indices (48-bit VA, 4KB pages)
            l0_idx = (x8_pre_branch >> 39) & 0x1FF
            l1_idx = (x8_pre_branch >> 30) & 0x1FF
            l2_idx = (x8_pre_branch >> 21) & 0x1FF
            l3_idx = (x8_pre_branch >> 12) & 0x1FF
            page_off = x8_pre_branch & 0xFFF

            print(f"  VA Indices: L0={l0_idx} (0x{l0_idx:x}), L1={l1_idx} (0x{l1_idx:x}), L2={l2_idx} (0x{l2_idx:x}), L3={l3_idx} (0x{l3_idx:x}), PageOffset=0x{page_off:x}", flush=True)

            curr_tbl_pa = base_pa
            for lvl, idx in [(0, l0_idx), (1, l1_idx), (2, l2_idx), (3, l3_idx)]:
                desc_addr = curr_tbl_pa + (idx * 8)
                desc_bytes = get_mem(s, desc_addr, 8, "p01.02")
                desc_val = struct.unpack('<Q', desc_bytes)[0] if len(desc_bytes) == 8 else 0
                
                valid = (desc_val & 1) != 0
                is_table = ((desc_val & 3) == 3) if lvl < 3 else False
                is_block = ((desc_val & 3) == 1) if lvl < 3 else ((desc_val & 3) == 3)
                
                oa = (desc_val & 0x0000FFFFFFFFF000)
                ap = (desc_val >> 6) & 3
                uxn = (desc_val >> 54) & 1
                pxn = (desc_val >> 53) & 1
                attr_idx = (desc_val >> 2) & 7
                sh = (desc_val >> 8) & 3

                desc_type = "Table" if is_table else ("Block/Page" if is_block else "Invalid/Unmapped")

                print(f"\n  [Level-{lvl} Descriptor @ PA 0x{desc_addr:016x}]:", flush=True)
                print(f"    Raw Value:     0x{desc_val:016x}", flush=True)
                print(f"    Valid Bit:     {valid}", flush=True)
                print(f"    Type:          {desc_type}", flush=True)
                print(f"    Output PA:     0x{oa:016x}", flush=True)
                print(f"    Access Perms:  AP={ap} (0b{ap:02b})", flush=True)
                print(f"    Execute Never: UXN={uxn}, PXN={pxn}", flush=True)
                print(f"    AttrIndex:     {attr_idx}, Shareability={sh}", flush=True)

                if not valid:
                    print(f"\n  ===> LEVEL-{lvl} DESCRIPTOR IS UNMAPPED (INVALID)! Walk Terminated.", flush=True)
                    break

                if is_block:
                    final_pa = oa | (x8_pre_branch & ((1 << (30 if lvl==1 else (21 if lvl==2 else 12))) - 1))
                    print(f"\n  ===> FINAL TRANSLATED PHYSICAL ADDRESS: 0x{final_pa:016x} (via Level-{lvl} Block)", flush=True)
                    break

                curr_tbl_pa = oa

            print("============================================================================\n", flush=True)
            break

        # Step vCPU1
        send_rsp(s, "vCont;s:p01.02")

    time.sleep(0.5)
    s.close()
finally:
    proc.terminate()
    print("\n[Test Complete]", flush=True)
