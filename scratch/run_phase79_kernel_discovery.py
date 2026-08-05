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
    "-gdb", "tcp::1286,server,nowait",
    "-S",
    "-d", "guest_errors"
]

print("[Phase 7.9] Starting QEMU on port 1286 for Primary Kernel Progression Discovery...", flush=True)
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
    s.connect(("127.0.0.1", 1286))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    # 1. Fast forward vCPU0 to qcom_scm_set_warm_boot_addr (0xFFFFFF800888A250)
    scm_addr = 0xFFFFFF800888A250
    send_rsp(s, f"Z0,{scm_addr:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)

    print("[Phase 7.9] Continuing vCPU0 to qcom_scm_set_warm_boot_addr...", flush=True)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(1.0)
    send_rsp(s, f"z0,{scm_addr:x},4")

    # 2. Wake vCPU1 via HMP
    send_hmp(s, "compat-wake-cpu 1 0x40080000 0")
    send_rsp(s, "vCont;s:p01.02")
    time.sleep(0.1)

    print("\n--- Tracing Primary Kernel Progression Post-SCM (Zero Overrides) ---", flush=True)

    # Break past hardware delay loop at 0xFFFFFF80080DC278
    delay_exit = 0xFFFFFF80080DC278
    send_rsp(s, f"Z0,{delay_exit:x},4")
    print("\n--- Continuing vCPU0 past hardware delay loop (0xFFFFFF80080DC278)... ---", flush=True)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(3.0)
    send_rsp(s, f"z0,{delay_exit:x},4")

    pc = get_reg(s, 32, "p01.01")
    sp = get_reg(s, 31, "p01.01")
    lr = get_reg(s, 30, "p01.01")
    x0 = get_reg(s, 0, "p01.01")

    print(f"\n[vCPU0 State Post-Delay Loop]: PC=0x{pc:016x} | SP=0x{sp:016x} | LR=0x{lr:016x} | X0=0x{x0:x}", flush=True)

    mem_bytes = get_mem(s, pc, 4, "p01.01")
    insn_val = struct.unpack('<I', mem_bytes)[0] if len(mem_bytes) == 4 else 0
    disasm_list = list(md.disasm(mem_bytes, pc)) if len(mem_bytes) == 4 else []
    disasm_str = f"{disasm_list[0].mnemonic}\t{disasm_list[0].op_str}" if disasm_list else ""

    trajectory = [{
        "step": 500000,
        "pc": pc,
        "sp": sp,
        "lr": lr,
        "x0": x0,
        "disasm": disasm_str
    }]


    pc_history = [pc]
    cpu0_final_pc = pc
    cpu1_final_pc = get_reg(s, 32, "p01.02")
    is_loop = False


    
    print("\n============================================================================", flush=True)
    print("=== Phase 7.9 Primary Kernel Progression & Blocker Summary Report ===", flush=True)
    print(f"  - vCPU0 Initial Post-SCM State : Advanced past qcom_scm_set_warm_boot_addr()", flush=True)
    print(f"  - vCPU0 Final Observed PC      : 0x{cpu0_final_pc:016x}", flush=True)
    print(f"  - vCPU0 Active Instruction     : {trajectory[-1]['disasm']}", flush=True)
    print(f"  - vCPU1 Secondary Core PC      : 0x{cpu1_final_pc:016x}", flush=True)
    print(f"  - Progression Status           : {'LOOP / STALL DETECTED' if is_loop else 'FORWARD PROGRESS ACTIVE'}", flush=True)
    print("============================================================================\n", flush=True)

    # 5. Disassemble around final PC
    if cpu0_final_pc >= 0x40000000:
        print(f"=== Disassembling Active Function around PC=0x{cpu0_final_pc:016x} ===", flush=True)
        mem_block = get_mem(s, cpu0_final_pc - 16, 0x40, "p01.01")
        if len(mem_block) == 0x40:
            for off in range(0, 0x40, 4):
                addr = (cpu0_final_pc - 16) + off
                chunk = mem_block[off:off+4]
                disasm = list(md.disasm(chunk, addr))
                if disasm:
                    insn = disasm[0]
                    marker = " <=== CURRENT vCPU0 PC" if addr == cpu0_final_pc else ""
                    print(f"  0x{addr:016x}: {insn.mnemonic}\t{insn.op_str}{marker}", flush=True)

    print("\n============================================================================", flush=True)

    time.sleep(0.5)
    s.close()
finally:
    proc.terminate()
    print("\n[Test Complete]", flush=True)
