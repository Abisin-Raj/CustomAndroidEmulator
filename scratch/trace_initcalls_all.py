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
    "-gdb", "tcp::1312,server,nowait",
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
    s.settimeout(10.0)
    s.connect(("127.0.0.1", 1312))
    s.sendall(b"+")

    # Fast-forward vCPU0 past boot to 0xffffff800a4f2000 (do_initcall_level)
    bp_addr = 0xffffff800a4f2000
    send_rsp(s, f"Z0,{bp_addr:x},4")
    send_rsp(s, "vCont;s:p01.01")
    time.sleep(0.1)
    send_rsp(s, "vCont;c:p01.01")
    time.sleep(2.0)
    send_rsp(s, f"z0,{bp_addr:x},4")

    # Wake vCPU1
    send_hmp(s, "compat-wake-cpu 1 0x40080000 0")
    send_rsp(s, "vCont;s:p01.02")
    time.sleep(0.1)

    print("\n=== Tracing All Initcalls (blr x9 @ 0xffffff800a4f204c) ===", flush=True)

    # Let's set breakpoint on blr x9 which is at 0xffffff800a4f204c
    # wait! Let's check from our disassembly:
    # 0xffffff800a4f204c: tbnz	w6, #0, #0xffffff800a4f1ec0
    # Wait, where is blr x9?
    # 0xffffff800a4f204c: tbnz...
    # Ah! In the disassembly:
    # 0xffffff800a4f2044: b.eq	#0xffffff800a4f2050
    # 0xffffff800a4f2048: ldrb	w6, [x6]
    # 0xffffff800a4f204c: tbnz	w6, #0, #0xffffff800a4f1ec0
    # Wait, where was the blr x9?
    # Ah! In the previous printout:
    # 0xffffff800a4f2040: ccmp	x6, #0, #4, ne
    # 0xffffff800a4f2044: b.eq	#0xffffff800a4f2050
    # Wait, where did the blr x9 go?
    # Let's check the disassembly of 0xffffff800a4f2030 to 0xffffff800a4f2060:
    # 0xffffff800a4f203c: add	x6, x6, x19, lsl #5
    # 0xffffff800a4f2040: ccmp	x6, #0, #4, ne
    # 0xffffff800a4f2044: b.eq	#0xffffff800a4f2050
    # Wait, is there a blr x9 at 0xffffff800a4f204c? No, the disassembly showed tbnz!
    # Ah! Where was the blr x9?
    # Let's check:
    # 0xffffff800a4f2024: cbz	x5, #0xffffff800a4f209c
    # 0xffffff800a4f2028: lsr	x6, x3, #0x16
    # 0xffffff800a4f202c: ubfx	x19, x3, #0x12, #7
    # 0xffffff800a4f2030: and	x6, x6, #0x3fffffffff8
    # 0xffffff800a4f2034: ldr	x6, [x5, x6]
    # 0xffffff800a4f2038: cmp	x6, #0
    # Wait, let's look at the rest of do_initcall_level!
    # Let's disassemble from 0xffffff800a4f2050 to 0xffffff800a4f20d0.
    
    # Actually, we can just set a breakpoint at the entry of do_one_initcall if we can find it.
    # But wait! We know the loop calls the initcall. Let's find where the blr is.
    # Let's read memory of 0xffffff800a4f2040 to 0xffffff800a4f20c0 and look for the blr instruction.
    
    # Or, we can just disassemble the instructions and find the address of any 'blr' instruction in this range!
    
finally:
    s.close()
    proc.terminate()
