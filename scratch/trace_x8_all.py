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
    "-gdb", "tcp::1316,server,nowait",
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
    s.connect(("127.0.0.1", 1316))
    s.sendall(b"+")

    # Set breakpoint at blr x8 (0xFFFFFF80080DC230)
    bp_addr = 0xFFFFFF80080DC230
    send_rsp(s, f"Z0,{bp_addr:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)

    print("=== Tracing All blr x8 Calls ===", flush=True)

    # Let's run and catch the first 5 hits of the breakpoint
    for hit in range(1, 6):
        send_rsp(s, "vCont;c:p01.01")
        time.sleep(0.5)
        
        pc = get_reg(s, 32, "p01.01")
        if pc != bp_addr:
            print(f"Hit {hit} did not stop at breakpoint. Stopped at PC=0x{pc:016x}", flush=True)
            break
            
        x8 = get_reg(s, 8, "p01.01")
        w20 = get_reg(s, 20, "p01.01")
        
        # Step once to execute blr x8
        send_rsp(s, "vCont;s:p01.01")
        time.sleep(0.05)
        
        # Read PC after step
        pc_after = get_reg(s, 32, "p01.01")
        
        # Step 2 more times to return from call
        send_rsp(s, "vCont;s:p01.01")
        time.sleep(0.05)
        send_rsp(s, "vCont;s:p01.01")
        time.sleep(0.05)
        
        x0_ret = get_reg(s, 0, "p01.01")
        print(f"Call #{hit}: Target X8=0x{x8:016x} | Arg W20=0x{w20:x} | Branched to PC=0x{pc_after:016x} | Returned X0=0x{x0_ret:x}", flush=True)

    send_rsp(s, f"z0,{bp_addr:x},4")

finally:
    s.close()
    proc.terminate()
