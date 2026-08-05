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
    "-gdb", "tcp::1238,server,nowait",
    "-S",
    "-d", "guest_errors"
]

print("[Phase 7.2A] Starting QEMU with Real PixelExperience Kernel Image on port 1238...", flush=True)
proc = subprocess.Popen(cmd)
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

def get_pc(s, thread="p01.02"):
    send_rsp(s, f"Hg{thread}")
    resp = send_rsp(s, "p20")
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
    s.connect(("127.0.0.1", 1238))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    # 1. Inspect initial kernel _text entry point on vCPU0
    cpu0_pc = get_pc(s, "p01.01")
    print(f"[Phase 7.2A] vCPU0 Initial PC: 0x{cpu0_pc:016x}", flush=True)

    # 2. Issue compat-wake-cpu 1 0x40080000 (or physical kernel RAM entry 0x40080000)
    # QEMU virt machine loads kernel at PA 0x40080000
    entry_addr = 0x40080000
    res = send_hmp(s, f"compat-wake-cpu 1 0x{entry_addr:x} 0")
    print(f"[Phase 7.2A HMP]: {res.strip()}", flush=True)

    # 3. Perform 100-step adaptive single-step trace on vCPU1
    print("\n--- Phase 7.2A: Adaptive 100-Instruction Execution Trace (vCPU1) ---", flush=True)
    
    final_pc = 0
    total_steps = 0
    loop_detected = False
    stop_reason = "Max 100 steps reached"
    pc_freq = {}

    for step in range(1, 101):
        pc = get_pc(s, "p01.02")
        mem_bytes = get_mem(s, pc, 4, "p01.02") if pc != 0 else b""
        insn_val = struct.unpack('<I', mem_bytes)[0] if len(mem_bytes) == 4 else 0
        
        # Simple symbol resolution heuristics for kernel RAM offset
        offset_from_ram = pc - 0x40000000 if pc >= 0x40000000 else pc
        sym_name = f"kernel_RAM+0x{offset_from_ram:x}"
        if pc == 0x40080000:
            sym_name = "kernel_head.S (_text entry)"
        elif pc == 0x200 or (pc & 0xFFF) == 0x200:
            sym_name = "ARM64_EL1_Sync_Exception_Vector (0x200)"

        print(f"[Phase 7.2A Step {step:03d}/100] PC=0x{pc:016x} | Symbol: {sym_name} | Opcode=0x{insn_val:08x}", flush=True)

        final_pc = pc
        total_steps = step
        pc_freq[pc] = pc_freq.get(pc, 0) + 1

        if pc_freq[pc] >= 5:
            loop_detected = True
            stop_reason = f"Loop detected at PC 0x{pc:016x} [{sym_name}]"
            print(f"[Phase 7.2A Stop]: {stop_reason}", flush=True)
            break

        if pc == 0x200 or (pc & 0xFFF) == 0x200:
            stop_reason = "Trapped to ARM64 Synchronous Exception Vector (0x200)"
            print(f"[Phase 7.2A Stop]: {stop_reason}", flush=True)
            break

        # Step vCPU1
        send_rsp(s, "vCont;s:p01.02")

    print("\n============================================================================", flush=True)
    print("=== CPU1 Trace Summary (Phase 7.2A) ===", flush=True)
    print(f"  Final PC:              0x{final_pc:016x}", flush=True)
    print(f"  Symbol:                {sym_name}", flush=True)
    print(f"  Instructions Executed: {total_steps}", flush=True)
    print(f"  Loop Detected:         {'YES' if loop_detected else 'NO'}", flush=True)
    print(f"  Stop Reason:           {stop_reason}", flush=True)
    print("============================================================================", flush=True)

    time.sleep(0.5)
    s.close()
finally:
    proc.terminate()
    print("\n[Test Complete]", flush=True)
