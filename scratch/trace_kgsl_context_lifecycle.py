import socket
import struct
import time
import subprocess

# Stop existing qemu
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

    gdb = socket.socket()
    gdb.connect(('127.0.0.1', 1999))
    gdb.sendall(b'+$qSupported:multiprocess+;swbreak+;hwbreak+#32')
    gdb.recv(1024)
    gdb.sendall(b'+$?#3f')
    gdb.recv(1024)

    def recv_pkt():
        data = bytearray()
        while True:
            c = gdb.recv(1)
            if not c: return None
            if c == b'+': continue
            if c == b'$':
                while True:
                    ch = gdb.recv(1)
                    data.extend(ch)
                    if ch == b'#':
                        data.extend(gdb.recv(2))
                        gdb.sendall(b'+')
                        return bytes(data)

    def send_cmd(cmd):
        csum = sum(cmd.encode('latin1')) % 256
        gdb.sendall(f"${cmd}#{csum:02x}".encode('latin1'))
        return recv_pkt()

    BP = 0xffffff8008b34d50
    send_cmd(f'Z0,{BP:x},4')
    csum = sum(b'c') % 256
    gdb.sendall(f"$c#{csum:02x}".encode('latin1'))

    # Wait for stop
    stop_pkt = recv_pkt()
    print('Stopped at BP:', stop_pkt)

    # Read registers
    regs_pkt = send_cmd('g')
    hex_data = regs_pkt.split(b'$')[1].split(b'#')[0].decode('ascii')
    regs = [struct.unpack('<Q', bytes.fromhex(hex_data[i:i+16]))[0] for i in range(0, len(hex_data), 16) if len(hex_data[i:i+16]) == 16]

    dev_priv = regs[0]
    cmd_req = regs[2]
    print(f'dev_priv: 0x{dev_priv:016x}, cmd_req: 0x{cmd_req:016x}')

    # Read struct kgsl_gpu_command (64 bytes)
    cmd_mem_pkt = send_cmd(f'm{cmd_req:x},40')
    cmd_hex = cmd_mem_pkt.split(b'$')[1].split(b'#')[0].decode('ascii')
    cmd_bytes = bytes.fromhex(cmd_hex)
    cid = struct.unpack('<I', cmd_bytes[56:60])[0]
    ts = struct.unpack('<I', cmd_bytes[60:64])[0]
    print(f'cmd_req fields: context_id={cid}, timestamp={ts}')

    # Read dev_priv->device (offset 0)
    dev_mem = send_cmd(f'm{dev_priv:x},10')
    dev_hex = dev_mem.split(b'$')[1].split(b'#')[0].decode('ascii')
    device_ptr = struct.unpack('<Q', bytes.fromhex(dev_hex)[:8])[0]
    proc_priv = struct.unpack('<Q', bytes.fromhex(dev_hex)[8:16])[0]
    print(f'device_ptr: 0x{device_ptr:016x}, proc_priv: 0x{proc_priv:016x}')

    # Read context_idr at device_ptr + 0x8e0
    idr_mem = send_cmd(f'm{device_ptr+0x8e0:x},20')
    idr_hex = idr_mem.split(b'$')[1].split(b'#')[0].decode('ascii')
    idr_bytes = bytes.fromhex(idr_hex)
    gfp_mask, rnode, idr_base, idr_next = struct.unpack('<QQII', idr_bytes[:24])
    print(f'device->context_idr: gfp_mask=0x{gfp_mask:x}, rnode=0x{rnode:016x}, idr_base={idr_base}, idr_next={idr_next}')

    # Also inspect proc_priv->ctxt_count and proc_priv->ctxt_count_lock
    # In kgsl_process_private:
    # let's read first 64 bytes of proc_priv
    proc_mem = send_cmd(f'm{proc_priv:x},40')
    proc_hex = proc_mem.split(b'$')[1].split(b'#')[0].decode('ascii')
    print(f'proc_priv first 64 bytes: {proc_hex}')

    # If rnode != 0, examine what is in the radix tree!
    if rnode != 0:
        print(f'Radix tree rnode is non-null! Inspecting rnode at 0x{rnode:x}')
        rnode_clean = rnode & ~3
        rnode_mem = send_cmd(f'm{rnode_clean:x},40')
        print(f'rnode header: {rnode_mem.split(b"$")[1].split(b"#")[0].decode("ascii")}')

finally:
    try: qmp.close()
    except: pass
    try: gdb.close()
    except: pass
    proc.kill()
    subprocess.run(['powershell', '-Command', 'Get-Process qemu-system-aarch64 -ErrorAction SilentlyContinue | Stop-Process -Force'], capture_output=True)