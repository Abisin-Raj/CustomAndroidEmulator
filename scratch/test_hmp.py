import socket
import time
import subprocess
import sys
import struct

qemu_bin = r"C:\qemu_source\build\qemu-system-aarch64.exe"
cmd = [
    qemu_bin,
    "-machine", "virt",
    "-cpu", "cortex-a76,psci-conduit=1",
    "-smp", "2",
    "-m", "4096",
    "-nographic",
    "-gdb", "tcp::1235,server,nowait",
    "-S",
    "-d", "guest_errors"
]

print("[Test] Starting QEMU on port 1235...", flush=True)
proc = subprocess.Popen(cmd)
time.sleep(1)

def send_rsp(s, payload):
    checksum = sum(payload.encode('utf-8')) % 256
    pkt = f"${payload}#{checksum:02x}"
    s.sendall(pkt.encode('utf-8'))
    
    resp_accum = ""
    while True:
        try:
            chunk = s.recv(2048).decode(errors='ignore')
            if not chunk:
                break
            resp_accum += chunk
            # If we received a data packet ending with checksum or +, break
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

def get_pc(s):
    send_rsp(s, "Hgp01.02")
    resp = send_rsp(s, "p20")
    # Extract hex string after $ and before #
    for part in resp.split('$'):
        if part and '#' in part:
            raw = part.split('#')[0]
            if len(raw) >= 16:
                try:
                    val_bytes = bytes.fromhex(raw[:16])
                    pc_val = struct.unpack('<Q', val_bytes)[0]
                    return pc_val
                except Exception:
                    pass
    return 0

def get_mem(s, addr, size):
    send_rsp(s, "Hgp01.02")
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
    s.settimeout(3.0)
    s.connect(("127.0.0.1", 1235))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    # 1. Wake CPU1 at 0x82000600
    res = send_hmp(s, "compat-wake-cpu 1 0x82000600 0")
    print("[HMP compat-wake-cpu]:", res.strip(), flush=True)

    # 2. Perform 20 single-steps on vCPU1
    print("\n--- Phase 7.1: 20-Instruction vCPU1 Execution Window ---", flush=True)
    for step in range(1, 21):
        pc = get_pc(s)
        mem_bytes = get_mem(s, pc, 4) if pc != 0 else b""
        insn_val = struct.unpack('<I', mem_bytes)[0] if len(mem_bytes) == 4 else 0
        
        print(f"[Phase 7.1 Step {step:02d}/20] PC=0x{pc:016x} | Opcode=0x{insn_val:08x}", flush=True)

        # Single step vCPU1
        send_rsp(s, "vCont;s:p01.02")

    time.sleep(0.5)
    s.close()
finally:
    proc.terminate()
    print("\n[Test Complete]", flush=True)
