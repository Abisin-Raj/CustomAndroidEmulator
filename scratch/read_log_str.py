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
    "-gdb", "tcp::1272,server,nowait",
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

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect(("127.0.0.1", 1272))
    s.sendall(b"+")

    # Fast-forward vCPU0 to 0xFFFFFF800A4F2990
    send_rsp(s, "Z0,ffffff800a4f2990,4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(1.5)
    send_rsp(s, "z0,ffffff800a4f2990,4")

    # Read strings
    s0 = get_mem(s, 0xFFFFFF800A8AF9D0, 128, "p01.01").split(b'\x00')[0].decode(errors='ignore')
    s1 = get_mem(s, 0xFFFFFF800A16C7A3, 128, "p01.01").split(b'\x00')[0].decode(errors='ignore')

    print(f"\n============================================================================", flush=True)
    print(f"=== vCPU0 Kernel Init Log Strings @ 0xFFFFFF800A4F2990 ===", flush=True)
    print(f"  String 1 (0xFFFFFF800A8AF9D0): \"{s0}\"", flush=True)
    print(f"  String 2 (0xFFFFFF800A16C7A3): \"{s1}\"", flush=True)
    print("============================================================================\n", flush=True)




finally:
    s.close()
    proc.terminate()
