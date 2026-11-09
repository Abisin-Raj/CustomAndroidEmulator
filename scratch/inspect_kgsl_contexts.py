import socket
import struct
import time
import subprocess

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

class GDBClient:
    def __init__(self, port=1999, timeout=10.0):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(timeout)
        self.sock.connect(("127.0.0.1", port))
        self._drain()
        self.sock.sendall(b"+")
        self._send_raw("qSupported:multiprocess+;swbreak+;hwbreak+")
        self._send_raw("?")

    def _drain(self):
        self.sock.setblocking(False)
        try:
            while True:
                d = self.sock.recv(4096)
                if not d: break
        except: pass
        self.sock.setblocking(True)

    def _recv_pkt(self, timeout=10.0):
        self.sock.settimeout(timeout)
        data = bytearray()
        while True:
            try: c = self.sock.recv(1)
            except socket.timeout: return None
            if not c: return None
            if c == b'+': continue
            if c == b'$':
                data = bytearray(b'$')
                while True:
                    ch = self.sock.recv(1)
                    if not ch: break
                    data.extend(ch)
                    if ch == b'#':
                        data.extend(self.sock.recv(2))
                        self.sock.sendall(b'+')
                        return bytes(data)

    def _send_raw(self, cmd):
        csum = sum(cmd.encode('latin1')) % 256
        pkt = f"${cmd}#{csum:02x}".encode('latin1')
        self.sock.sendall(pkt)
        return self._recv_pkt()

    def set_bp(self, addr):
        res = self._send_raw(f"Z0,{addr:x},4")
        return res is not None and b'OK' in res

    def remove_bp(self, addr):
        res = self._send_raw(f"z0,{addr:x},4")
        return res is not None and b'OK' in res

    def cont_async(self):
        self._drain()
        csum = sum(b'c') % 256
        self.sock.sendall(f"$c#{csum:02x}".encode('latin1'))

    def wait_stop(self, timeout=10.0):
        return self._recv_pkt(timeout=timeout)

    def get_regs(self):
        raw = self._send_raw("g")
        if not raw or b'$' not in raw: return []
        try:
            hex_data = raw.split(b'$')[1].split(b'#')[0].decode('ascii')
            return [struct.unpack('<Q', bytes.fromhex(hex_data[i:i+16]))[0]
                    for i in range(0, len(hex_data), 16) if len(hex_data[i:i+16]) == 16]
        except: return []

    def read_mem(self, addr, length):
        if addr == 0 or length <= 0: return b""
        raw = self._send_raw(f"m{addr:x},{length:x}")
        if not raw or b'$' not in raw: return b""
        try:
            hex_data = raw.split(b'$')[1].split(b'#')[0].decode('ascii')
            if hex_data.startswith('E'): return b""
            return bytes.fromhex(hex_data)
        except: return b""

    def read_u64(self, addr):
        b = self.read_mem(addr, 8)
        return struct.unpack('<Q', b)[0] if len(b) == 8 else None

    def read_u32(self, addr):
        b = self.read_mem(addr, 4)
        return struct.unpack('<I', b)[0] if len(b) == 4 else None

    def close(self):
        try: self.sock.close()
        except: pass

try:
    qmp = socket.socket()
    qmp.connect(('127.0.0.1', 4499))
    qmp.recv(1024)
    qmp.sendall(b'{"execute":"qmp_capabilities"}\r\n')
    qmp.recv(1024)
    qmp.sendall(b'{"execute":"human-monitor-command","arguments":{"command-line":"loadvm cp02_sf_stable"}}\r\n')
    while True:
        chunk = qmp.recv(4096)
        if b'return' in chunk: break

    gdb = GDBClient()

    # kgsl_driver is at 0xffffff800a722b28
    # struct kgsl_driver has kgsl_devices[0] at offset 0x48 or 0x60
    # Let's inspect kgsl_driver memory
    drv_mem = gdb.read_mem(0xffffff800a722b28, 256)
    print("kgsl_driver first 64B:", drv_mem[:64].hex())

    # Stop at kgsl_ioctl_gpu_command to get exact device ptr
    BP_ENTRY = 0xffffff8008b34d50
    gdb.set_bp(BP_ENTRY)
    gdb.cont_async()
    pkt = gdb.wait_stop(timeout=15.0)

    regs = gdb.get_regs()
    dev_priv = regs[0]
    cmd_req_ptr = regs[2]
    device_ptr = gdb.read_u64(dev_priv)
    proc_priv = gdb.read_u64(dev_priv + 8)

    print(f"dev_priv:    0x{dev_priv:016x}")
    print(f"device:      0x{device_ptr:016x}")
    print(f"proc_priv:   0x{proc_priv:016x}")

    if proc_priv:
        pid = gdb.read_u32(proc_priv + 0x2c) # or check comm string
        raw_comm = gdb.read_mem(proc_priv, 64)
        print(f"proc_priv memory: {raw_comm.hex()}")

    # Check context_idr in device
    # In kgsl_context_get:
    # 0xffffff8008b34224: ldr x0, [x23, #0x8e8]
    # x23 is device_ptr! So context_idr root is at device_ptr + 0x8e8!
    idr_root = gdb.read_u64(device_ptr + 0x8e8)
    print(f"context_idr root: 0x{idr_root:016x}")

    if idr_root:
        rnode_addr = idr_root & ~3
        raw = gdb.read_mem(rnode_addr, 576)
        shift, offset, count, exceptional = struct.unpack('<BBBB', raw[:4])
        print(f"RadixNode at 0x{rnode_addr:x}: shift={shift}, offset={offset}, count={count}, exceptional={exceptional}")
        print("Non-zero entries in radix node:")
        for off in range(0, len(raw), 8):
            val = struct.unpack('<Q', raw[off:off+8])[0]
            if val != 0:
                print(f"  +0x{off:03x}: 0x{val:016x}")

    # Inspect Context 1 and Context 2
    for cid, cptr in [(1, 0xffffffc0f1711000), (2, 0xffffffc0f1713800)]:
        cmem = gdb.read_mem(cptr, 64)
        if cmem and len(cmem) == 64:
            refcount, ctx_id, priority, tid = struct.unpack('<IIII', cmem[:16])
            ctx_dev_priv, ctx_proc_priv, ctx_priv, ctx_device = struct.unpack('<QQQQ', cmem[16:48])
            print(f"\n[Context {cid}] at 0x{cptr:016x}:")
            print(f"  id={ctx_id}, refcount={refcount}, priority={priority}, tid={tid}")
            print(f"  dev_priv=0x{ctx_dev_priv:016x} (calling dev_priv was 0x{dev_priv:016x})")
            print(f"  proc_priv=0x{ctx_proc_priv:016x}")
            print(f"  priv=0x{ctx_priv:016x} (DETACHED bit: {ctx_priv & 1})")
            print(f"  device=0x{ctx_device:016x}")
    for ctx_i in range(8):
        # Read idr_root
        print(f"  Context {ctx_i}: IDR root = 0x{idr_root:x}")

finally:
    try: qmp.close()
    except: pass
    try: gdb.close()
    except: pass
    subprocess.run(['powershell', '-Command', 'Get-Process qemu-system-aarch64 -ErrorAction SilentlyContinue | Stop-Process -Force'], capture_output=True)

