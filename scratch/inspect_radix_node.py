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

    device_ptr = 0xffffff800a723398
    idr_root_val = gdb.read_u64(device_ptr + 0x8e8)
    print(f"Dynamic idr_root = 0x{idr_root_val:016x}")
    rnode_addr = idr_root_val & ~3
    raw = gdb.read_mem(rnode_addr, 576)
    shift, offset, count, exceptional = struct.unpack('<BBBB', raw[:4])
    print(f'RadixNode at 0x{rnode_addr:x}: shift={shift}, offset={offset}, count={count}, exceptional={exceptional}')

    print('Scanning non-zero entries in radix node (offset 0 to 576):')
    for off in range(0, len(raw), 8):
        val = struct.unpack('<Q', raw[off:off+8])[0]
        if val != 0:
            print(f'  +0x{off:03x}: 0x{val:016x}')

    # Let's inspect where slots start by tracing the inlined assembly!
    # In kgsl_context_get:
    # 0xffffff8008b3425c: and x10, x0, #0xfffffffffffffffe
    # 0xffffff8008b34260: ldrb w11, [x10] (shift)
    # 0xffffff8008b34264: lsr x12, x8, x11 (index >> shift)
    # 0xffffff8008b34268: and x12, x12, #0x3f
    # 0xffffff8008b3426c: add x10, x10, #0x28 (or #0x20?)
    # 0xffffff8008b34270: ldr x0, [x10, x12, lsl #3]!
    # Let's verify what offset is added to x10 in the kernel disassembly!

finally:
    try: qmp.close()
    except: pass
    try: gdb.close()
    except: pass
    proc.kill()
    subprocess.run(['powershell', '-Command', 'Get-Process qemu-system-aarch64 -ErrorAction SilentlyContinue | Stop-Process -Force'], capture_output=True)
