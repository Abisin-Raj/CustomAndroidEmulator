import socket
import time
import subprocess
import sys

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
    resp = ""
    while True:
        try:
            chunk = s.recv(1024).decode(errors='ignore')
            resp += chunk
            if '#' in chunk or '+' in chunk:
                break
        except Exception:
            break
    s.sendall(b"+")
    return resp

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

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(3.0)
    s.connect(("127.0.0.1", 1235))
    s.sendall(b"+")
    send_rsp(s, "qSupported:multiprocess+")

    # 1. Inspect initial CPUs
    cpus_before = send_hmp(s, "info cpus")
    print("[HMP info cpus BEFORE wake]:\n", cpus_before.strip(), flush=True)

    # 2. Wake CPU1 at 0x82000600
    res = send_hmp(s, "compat-wake-cpu 1 0x82000600 0")
    print("\n[HMP compat-wake-cpu]:\n", res.strip(), flush=True)

    # 3. Inspect CPUs after wake
    cpus_after = send_hmp(s, "info cpus")
    print("\n[HMP info cpus AFTER wake]:\n", cpus_after.strip(), flush=True)

    time.sleep(0.5)
    s.close()
finally:
    proc.terminate()
    print("\n[Test Complete]", flush=True)
