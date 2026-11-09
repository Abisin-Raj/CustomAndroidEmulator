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

    def step(self):
        csum = sum(b's') % 256
        self.sock.sendall(f"$s#{csum:02x}".encode('latin1'))
        return self._recv_pkt(timeout=5.0)

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

    # Set BP at kgsl_context_get return: 0xffffff8008b34dfc
    BP_AFTER_CTX_GET = 0xffffff8008b34dfc
    gdb.set_bp(BP_AFTER_CTX_GET)

    # Set BP at kgsl_ioctl_gpu_command return: 0xffffff8008b4a338
    BP_RET = 0xffffff8008b4a338
    gdb.set_bp(BP_RET)

    gdb.cont_async()
    pkt = gdb.wait_stop(timeout=15.0)
    print("Stopped:", pkt)

    regs = gdb.get_regs()
    pc = regs[32]
    print(f"Hit BP: PC=0x{pc:016x}, x0=0x{regs[0]:016x}")

    if pc == BP_AFTER_CTX_GET:
        print(f"Context pointer x0 = 0x{regs[0]:016x}")
        # Now step 50 instructions from here
        print("Stepping forward from after kgsl_context_get:")
        for step_i in range(50):
            gdb.step()
            s_regs = gdb.get_regs()
            s_pc = s_regs[32]
            print(f"  [{step_i:2d}] PC: 0x{s_pc:016x} | x0: 0x{s_regs[0]:x} x20: 0x{s_regs[20]:x} x24: 0x{s_regs[24]:x}")
            if s_pc == BP_RET:
                print(f"Reached return! Return value x0 = 0x{s_regs[0]:x}")
                break

finally:
    try: qmp.close()
    except: pass
    try: gdb.close()
    except: pass
    subprocess.run(['powershell', '-Command', 'Get-Process qemu-system-aarch64 -ErrorAction SilentlyContinue | Stop-Process -Force'], capture_output=True)
