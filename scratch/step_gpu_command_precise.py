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

    # Breakpoint at 0xffffff8008b34d78 (just before call to kgsl_context_get_owner)
    BP_CALL = 0xffffff8008b34d78
    BP_RET  = 0xffffff8008b34d7c

    gdb.set_bp(BP_CALL)
    gdb.cont_async()
    pkt = gdb.wait_stop(timeout=15.0)
    print("Hit BP_CALL:", pkt)

    regs = gdb.get_regs()
    print(f"Before call: x0=0x{regs[0]:016x}, w1={regs[1] & 0xffffffff}")

    # Set BP at return instruction
    gdb.set_bp(BP_RET)
    gdb.cont_async()
    pkt = gdb.wait_stop(timeout=5.0)
    print("Hit BP_RET:", pkt)

    regs_after = gdb.get_regs()
    print(f"After call:  x0=0x{regs_after[0]:016x} (returned context)")

    # If x0 is not NULL, let's see what happens next in kgsl_ioctl_gpu_command!
    # Let's single step 10 instructions
    pc = regs_after[32] # PC
    print(f"Current PC: 0x{pc:016x}")

    for step in range(15):
        reply = gdb._send_raw("s")
        regs_step = gdb.get_regs()
        if len(regs_step) > 32:
            spc = regs_step[32]
            print(f"  Step {step+1}: PC=0x{spc:016x}, x0=0x{regs_step[0]:016x}, x20=0x{regs_step[20]:016x}")

finally:
    try: qmp.close()
    except: pass
    try: gdb.close()
    except: pass
    proc.kill()
    subprocess.run(['powershell', '-Command', 'Get-Process qemu-system-aarch64 -ErrorAction SilentlyContinue | Stop-Process -Force'], capture_output=True)
