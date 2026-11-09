import socket
import struct
import time
import subprocess
from inspect_kgsl_contexts import GDBClient

subprocess.run(['powershell', '-Command', 'Get-Process qemu-system-aarch64 -ErrorAction SilentlyContinue | Stop-Process -Force'], capture_output=True)
time.sleep(1.0)

qemu_cmd = [
    r'C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidQEMU\build\qemu-system-aarch64.exe',
    '-L', r'C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidQEMU\pc-bios',
    '-M', 'virt,gic-version=3', '-cpu', 'cortex-a55', '-smp', '1', '-m', '4096',
    '-device', 'ramfb',
    '-kernel', r'C:\qemu_work\kernel_kgsl_d8_8.img',
    '-initrd', r'C:\qemu_work\boot_ramdisk_dri.gz',
    '-dtb', r'C:\qemu_work\virt_hw442.dtb',
    '-drive', r'file=C:\qemu_work\emmc_overlay_trace_scanout.qcow2,if=sd,format=qcow2,id=emmc0',
    '-display', 'none',
    '-qmp', 'tcp:127.0.0.1:4499,server=on,wait=off',
    '-gdb', 'tcp::1999',
    '-S'
]
proc = subprocess.Popen(qemu_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(2.0)

try:
    qmp = socket.socket()
    qmp.connect(('127.0.0.1', 4499))
    qmp.recv(1024)
    qmp.sendall(b'{"execute":"qmp_capabilities"}\r\n')
    qmp.recv(1024)
    qmp.sendall(b'{"execute":"human-monitor-command","arguments":{"command-line":"loadvm cp02_sf_stable"}}\r\n')
    while True:
        chunk = qmp.recv(4096)
        if b'return' in chunk:
            break

    gdb = GDBClient()

    BP_CALL = 0xffffff8008b34df8
    BP_RET  = 0xffffff8008b34dfc

    gdb.set_bp(BP_CALL)
    gdb.cont_async()
    pkt = gdb.wait_stop(timeout=15.0)

    regs = gdb.get_regs()
    print(f"Before call (0x{BP_CALL:x}): x0=0x{regs[0]:016x} (dev_priv), w1={regs[1] & 0xffffffff} (context_id)")

    gdb.set_bp(BP_RET)
    gdb.cont_async()
    pkt = gdb.wait_stop(timeout=5.0)

    regs_ret = gdb.get_regs()
    print(f"After call  (0x{BP_RET:x}):  x0=0x{regs_ret[0]:016x} (returned context pointer!)")

    # Trace next 15 instructions to see where it goes
    print("\nTracing post-call instructions:")
    for step in range(15):
        gdb._send_raw("s")
        rs = gdb.get_regs()
        if len(rs) > 32:
            print(f"  Step {step+1}: PC=0x{rs[32]:016x}, x0=0x{rs[0]:016x}, x20=0x{rs[20]:016x}")

finally:
    try: qmp.close()
    except: pass
    try: gdb.close()
    except: pass
    proc.kill()
    subprocess.run(['powershell', '-Command', 'Get-Process qemu-system-aarch64 -ErrorAction SilentlyContinue | Stop-Process -Force'], capture_output=True)

