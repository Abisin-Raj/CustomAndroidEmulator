import socket
import time
import subprocess
import os
import struct

qemu_bin = r"C:\qemu_source\build\qemu-system-aarch64.exe"
kernel_img = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\kernel.img"
sym_map_file = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\System.map.stub"

my_env = os.environ.copy()
my_env["PATH"] = r"C:\msys64\mingw64\bin;C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + my_env.get("PATH", "")

# Load Symbol Map
symbols = {}
if os.path.exists(sym_map_file):
    with open(sym_map_file, "r") as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 3:
                try:
                    addr = int(parts[0], 16)
                    sym_name = parts[2]
                    symbols[addr] = sym_name
                except ValueError:
                    pass

def resolve_symbol(addr):
    if addr in symbols:
        return symbols[addr]
    best_match = None
    best_diff = 0x10000
    for s_addr, s_name in symbols.items():
        if s_addr <= addr < s_addr + 0x1000:
            diff = addr - s_addr
            if diff < best_diff:
                best_diff = diff
                best_match = f"{s_name}+0x{diff:x}" if diff > 0 else s_name
    return best_match if best_match else f"0x{addr:016x}"

cmd = [
    qemu_bin,
    "-machine", "virt",
    "-cpu", "cortex-a76,psci-conduit=1",
    "-smp", "2",
    "-m", "4096",
    "-nographic",
    "-kernel", kernel_img,
    "-append", "console=ttyAMA0 earlycon=pl011,0x09000000 androidboot.hardware=qcom nokaslr",
    "-gdb", "tcp::1318,server,nowait",
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

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(20.0)
    s.connect(("127.0.0.1", 1318))
    s.sendall(b"+")

    # Fast-forward vCPU0 past memory initialization to the start of do_initcalls
    # wait! Where is do_initcalls?
    # In disasm_after.py, we saw do_initcall_level starts around 0xFFFFFF800A4F20D0 or similar?
    # Wait, the do_initcalls function in head.S or main.c starts at 0xFFFFFF80080DC008.
    initcalls_start = 0xFFFFFF80080DC008
    send_rsp(s, f"Z0,{initcalls_start:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)

    print("Fast-forwarding to do_initcalls (0xFFFFFF80080DC008)... This may take some time...", flush=True)
    send_rsp(s, "vCont;c:p01.01")
    
    # Wait up to 10 seconds to reach do_initcalls
    time.sleep(8.0)
    
    # Wake CPU1
    send_hmp(s, "compat-wake-cpu 1 0x40080000 0")
    time.sleep(0.5)

    pc = get_reg(s, 32, "p01.01")
    print(f"vCPU0 PC after run: 0x{pc:016x} -> {resolve_symbol(pc)}", flush=True)

    if pc == initcalls_start:
        print("Successfully reached do_initcalls!", flush=True)
    else:
        print("Did not reach do_initcalls yet. Let's see what is happening.", flush=True)

finally:
    s.close()
    proc.terminate()
