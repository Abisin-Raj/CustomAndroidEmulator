# trace_full_presentation_pipeline.py
# Traces BootAnimation presentation pipeline from CP-02 through every step to eglSwapBuffers
import socket
import json
import struct
import time
import sys
import vm_utils

sys.stdout.reconfigure(line_buffering=True)

QMP_PORT = 4444
GDB_PORT = 1776

class GDBClient:
    def __init__(self, host='127.0.0.1', port=1776):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        self.sock.sendall(b'+')
        self._init_session()

    def _recv_pkt(self, timeout=5.0):
        self.sock.settimeout(timeout)
        data = bytearray()
        while True:
            try:
                c = self.sock.recv(1)
            except socket.timeout:
                return None
            if not c:
                return None
            if c == b'+':
                continue
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

    def send_cmd(self, cmd, timeout=5.0):
        csum = sum(cmd.encode('latin1')) % 256
        pkt = f"${cmd}#{csum:02x}".encode('latin1')
        self.sock.sendall(pkt)
        return self._recv_pkt(timeout)

    def _init_session(self):
        self.send_cmd("qSupported:multiprocess+;swbreak+;hwbreak+")
        self.send_cmd("?")

    def get_regs(self):
        raw = self.send_cmd("g")
        if not raw or b'$' not in raw:
            return []
        try:
            hex_data = raw.split(b'$')[1].split(b'#')[0].decode('ascii')
            regs = []
            for i in range(0, len(hex_data)-15, 16):
                regs.append(struct.unpack('<Q', bytes.fromhex(hex_data[i:i+16]))[0])
            return regs
        except Exception:
            return []

    def set_bp(self, addr):
        res = self.send_cmd(f"Z0,{addr:x},4")
        return res and b'OK' in res

    def remove_bp(self, addr):
        res = self.send_cmd(f"z0,{addr:x},4")
        return res and b'OK' in res

    def step(self):
        csum = sum(b's') % 256
        self.sock.sendall(f"$s#{csum:02x}".encode('latin1'))
        return self._recv_pkt(timeout=10.0)

    def continue_async(self):
        csum = sum(b'c') % 256
        self.sock.sendall(f"$c#{csum:02x}".encode('latin1'))

    def wait_for_stop(self, timeout=1.0):
        return self._recv_pkt(timeout)

def main():
    print("=" * 80)
    print("=== FULL PRESENTATION PIPELINE TRACE (CP-02 -> EGLSWAPBUFFERS) ===")
    print("=" * 80)

    s_qmp, f_qmp = vm_utils.connect_qmp(QMP_PORT)
    print("Restoring CP-02 golden state (loadvm cp02_rtr_token)...")
    t_load = time.time()
    res_load = vm_utils.qmp_hmp(s_qmp, f_qmp, "loadvm cp02_rtr_token")
    print(f"loadvm completed in {time.time()-t_load:.2f}s: '{res_load.strip()}'")

    gdb = GDBClient(port=GDB_PORT)

    # Verify initial registers
    regs = gdb.get_regs()
    if regs and len(regs) > 32:
        pc = regs[32]
        sp = regs[31]
        lr = regs[30]
        print(f"Initial State: PC=0x{pc:x}, LR=0x{lr:x}, SP=0x{sp:x}")

    probes = {
        0x756f4ab310: {"id": "RET_TOKEN",        "name": "return from getInternalDisplayToken"},
        0x756f4ab34c: {"id": "CHK_TOKEN",        "name": "check dtoken (cbz x8, #0x756f4ab560)"},
        0x756f4ab388: {"id": "CALL_MODE",        "name": "call getActiveDisplayMode"},
        0x756f4ab390: {"id": "CHK_MODE",         "name": "check mode status (W0=status)"},
        0x756f4ab624: {"id": "CALL_SURF",        "name": "call createSurface"},
        0x756f4ab6cc: {"id": "CALL_TXN",         "name": "construct Transaction"},
        0x756f4aba34: {"id": "CALL_TXN_APPLY",   "name": "call Transaction::apply"},
        0x756f4aba38: {"id": "RET_TXN_APPLY",    "name": "return from Transaction::apply"},
        0x756f4aba4c: {"id": "CALL_EGL_GET_DPY", "name": "call eglGetDisplay"},
        0x756f4aba5c: {"id": "CALL_EGL_INIT",    "name": "call eglInitialize"},
        0x756f4aba60: {"id": "RET_EGL_INIT",     "name": "return from eglInitialize"},
        0x756f4aba94: {"id": "CALL_EGL_CHOOSE",  "name": "call eglChooseConfig"},
        0x756f4abab8: {"id": "CALL_EGL_WIN_SURF","name": "call eglCreateWindowSurface"},
        0x756f4ababc: {"id": "RET_EGL_WIN_SURF", "name": "return from eglCreateWindowSurface"},
        0x756f4abae8: {"id": "CALL_EGL_CTX",     "name": "call eglCreateContext"},
        0x756f4abaec: {"id": "RET_EGL_CTX",      "name": "return from eglCreateContext"},
        0x756f4abb08: {"id": "CALL_EGL_WIDTH",   "name": "call eglQuerySurface(EGL_WIDTH)"},
        0x756f4abb1c: {"id": "CALL_EGL_HEIGHT",  "name": "call eglQuerySurface(EGL_HEIGHT)"},
        0x756f4abb30: {"id": "CALL_MAKE_CURRENT","name": "call eglMakeCurrent"},
        0x756f4abb34: {"id": "RET_MAKE_CURRENT", "name": "return from eglMakeCurrent"},
        0x756f4abd44: {"id": "ERR_EXIT_RTR",     "name": "ERROR EXIT from readyToRun"},
        0x756f4abd74: {"id": "RET_RTR",          "name": "readyToRun successful return (W0=0)"},
        0x756f4ad550: {"id": "THREAD_LOOP",      "name": "BootAnimation::threadLoop entry"},
        0x756f4adb80: {"id": "BOOT_MOVIE",       "name": "BootAnimation::movie"},
        0x756f4b05c0: {"id": "CALL_SWAP_BUFFERS","name": "call eglSwapBuffers in movie"},
        0x756f4b44e0: {"id": "PLT_EGL_SWAP",     "name": "eglSwapBuffers PLT entry"},
        0x7562fa1c10: {"id": "DSO_EGL_SWAP",     "name": "libEGL.so::eglSwapBuffers entry"},
    }

    # Remove any stale breakpoints first
    for addr in list(probes.keys()) + [0x756f4ab30c]:
        gdb.remove_bp(addr)

    # Arm probes
    print("\nArming presentation pipeline probes:")
    for addr, p in probes.items():
        ok = gdb.set_bp(addr)
        print(f"  [ARMED] {p['id']:20s} @ 0x{addr:x} : {p['name']} (ok={ok})")

    # Step past initial breakpoint at 0x756f4ab30c
    print("\nSingle-stepping past initial pause (0x756f4ab30c)...")
    step_resp = gdb.step()
    regs = gdb.get_regs()
    if regs and len(regs) > 32:
        print(f"  After step: PC=0x{regs[32]:x}, LR=0x{regs[30]:x}, SP=0x{regs[31]:x}")

    print("\nContinuing execution with armed probes (120s observation window)...")
    gdb.continue_async()

    t0 = time.time()
    hits = []
    last_status = t0

    while time.time() - t0 < 120.0:
        elapsed = time.time() - t0
        if time.time() - last_status >= 10.0:
            last_status = time.time()
            print(f"  [STATUS @ {elapsed:5.1f}s] Observing... Total hits: {len(hits)}")

        pkt = gdb.wait_for_stop(timeout=1.0)
        if pkt:
            if b'$T' in pkt or b'$S' in pkt:
                regs = gdb.get_regs()
                if regs and len(regs) > 32:
                    pc = regs[32]
                    lr = regs[30]
                    sp = regs[31]
                    x0 = regs[0]
                    x1 = regs[1]
                    x8 = regs[8]
                    pinfo = probes.get(pc, None)
                    pid_str = pinfo["id"] if pinfo else f"UNK_0x{pc:x}"
                    pname = pinfo["name"] if pinfo else "Unknown"

                    print(f"\n[{elapsed:5.2f}s] *** HIT #{len(hits)+1}: {pid_str:20s} @ 0x{pc:x} : {pname} ***")
                    print(f"        -> X0=0x{x0:x}, X1=0x{x1:x}, X8=0x{x8:x}, LR=0x{lr:x}, SP=0x{sp:x}")
                    hits.append((elapsed, pid_str, pname, pc, lr, sp, x0, x1, x8))

                    if pid_str in ("CALL_SWAP_BUFFERS", "PLT_EGL_SWAP", "DSO_EGL_SWAP"):
                        print("\n" + "=" * 80)
                        print(f"SUCCESS: {pid_str} REACHED IN BOOTANIMATION!")
                        print("=" * 80)
                        break

                    # Step over breakpoint and rearm
                    gdb.remove_bp(pc)
                    gdb.step()
                    gdb.set_bp(pc)
                    gdb.continue_async()

    vm_utils.qmp_hmp(s_qmp, f_qmp, "stop")
    print(f"\nObservation window concluded. Total hits: {len(hits)}")
    print("=" * 80)
    for el, pid_str, name, pc, lr, sp, x0, x1, x8 in hits:
        print(f"  [{el:5.2f}s] {pid_str:20s} @ 0x{pc:x} : {name}")
        print(f"          X0=0x{x0:x}, X1=0x{x1:x}, X8=0x{x8:x}, LR=0x{lr:x}, SP=0x{sp:x}")
    print("=" * 80)

    # Disarm all probes
    for addr in probes:
        gdb.remove_bp(addr)

    s_qmp.close()
    print("Done.")

if __name__ == "__main__":
    main()
