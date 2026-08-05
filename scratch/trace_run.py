import socket
import time
import subprocess
import os
import struct

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
    "-gdb", "tcp::1308,server,nowait",
    "-S",
    "-d", "guest_errors"
]

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

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect(("127.0.0.1", 1308))
    s.sendall(b"+")

    # Fast-forward vCPU0 to the blr instruction @ 0xFFFFFF80080DC230
    bp_addr = 0xFFFFFF80080DC230
    send_rsp(s, f"Z0,{bp_addr:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.0)
    send_rsp(s, f"z0,{bp_addr:x},4")

    print("Fast-forwarded to 0xFFFFFF80080DC230. Now starting dynamic trace.", flush=True)

    # Let's trace the next 30,000 steps, recording how many times we visit blr x8, and what PC values we see
    visited_blr = 0
    pc_counts = {}
    
    for step in range(30000):
        send_rsp(s, "vCont;s:p01.01")
        pc = get_reg(s, 32, "p01.01")
        pc_counts[pc] = pc_counts.get(pc, 0) + 1
        
        if pc == 0xFFFFFF80080DC230:
            visited_blr += 1
            w20 = get_reg(s, 20, "p01.01")
            x0 = get_reg(s, 0, "p01.01")
            print(f"  Step {step:05d}: Visited blr x8. w20 (arg) = {w20}, X0 = 0x{x0:x}", flush=True)
            
        if pc == 0xFFFFFF80080DC370:
            print(f"  Step {step:05d}: Reached next initcall dispatch @ 0xFFFFFF80080DC370!", flush=True)
            break

    print(f"Trace complete. Total blr x8 visits: {visited_blr}", flush=True)
    # Print top 5 most visited PCs
    sorted_pcs = sorted(pc_counts.items(), key=lambda x: x[1], reverse=True)
    print("Top 5 most visited PC addresses:", flush=True)
    for p, c in sorted_pcs[:5]:
        print(f"  PC = 0x{p:016x} : {c} times", flush=True)

finally:
    s.close()
    proc.terminate()
