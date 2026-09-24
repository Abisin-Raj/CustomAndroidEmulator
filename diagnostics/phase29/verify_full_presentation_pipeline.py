# verify_full_presentation_pipeline.py
# Complete observation engine tracing BootAnimation -> EGL -> Gralloc -> SurfaceFlinger presentation loop
import socket
import struct
import time
import sys
import vm_utils

sys.stdout.reconfigure(line_buffering=True)
from monitor_dmesg_and_threads import fast_resolve, get_thread_info, parse_dmesg

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
        if not raw or b'$' not in raw: return []
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
        return self._recv_pkt(timeout=5.0)

    def continue_async(self):
        csum = sum(b'c') % 256
        self.sock.sendall(f"$c#{csum:02x}".encode('latin1'))

    def wait_for_stop(self, timeout=1.0):
        return self._recv_pkt(timeout)

def main():
    print("=" * 80)
    print("=== VERIFY FULL PRESENTATION PIPELINE (CP-02 -> EGLSWAPBUFFERS) ===")
    print("=" * 80)

    s_qmp, f_qmp = vm_utils.connect_qmp(QMP_PORT)
    print("Restoring CP-02 golden state (loadvm cp02_rtr_token)...")
    t_load = time.time()
    res_load = vm_utils.qmp_hmp(s_qmp, f_qmp, "loadvm cp02_rtr_token")
    print(f"loadvm completed in {time.time()-t_load:.2f}s: '{res_load.strip()}'")

    gdb = GDBClient(port=GDB_PORT)

    milestones = {
        0x756f4abab8: ("M1_CALL_WIN_SURF",   "call eglCreateWindowSurface"),
        0x756f4ababc: ("M2_RET_WIN_SURF",    "return eglCreateWindowSurface (X0=EGLSurface)"),
        0x756f4abaec: ("M3_RET_CREATE_CTX",  "return eglCreateContext (X0=EGLContext)"),
        0x756f4abb08: ("M4_CALL_QRY_W",      "call eglQuerySurface(EGL_WIDTH)"),
        0x756f4abb1c: ("M5_CALL_QRY_H",      "call eglQuerySurface(EGL_HEIGHT)"),
        0x756f4abb30: ("M6_CALL_MAKE_CURR",  "call eglMakeCurrent"),
        0x756f4abb34: ("M7_RET_MAKE_CURR",   "return eglMakeCurrent (W0=result)"),
        0x756f4abdfc: ("M_ERR_MAKE_CURR",    "ERROR: eglMakeCurrent failed (W0=0)"),
        0x756f4abd44: ("M_ERR_EXIT_RTR",     "ERROR: readyToRun error exit"),
        0x756f4abd74: ("M8_RET_RTR_SUCCESS", "SUCCESS: readyToRun returns 0"),
        0x756f4ad550: ("M9_ENTRY_TH_LOOP",   "BootAnimation::threadLoop entry"),
        0x756f4adb80: ("M10_ENTRY_MOVIE",    "BootAnimation::movie entry"),
        0x756f4b05c0: ("M11_CALL_SWAP",      "call eglSwapBuffers in movie"),
        0x756f4b44e0: ("M12_PLT_EGL_SWAP",   "eglSwapBuffers PLT entry"),
        0x7562fa1c10: ("M13_DSO_EGL_SWAP",   "libEGL.so::eglSwapBuffers entry"),
    }

    # Clear old breakpoints
    for addr in list(milestones.keys()) + [0x756f4ab30c, 0x756f4aba5c]:
        gdb.remove_bp(addr)

    print("\nArming presentation pipeline milestone probes:")
    for addr, (mid, mname) in milestones.items():
        ok = gdb.set_bp(addr)
        print(f"  [ARMED] {mid:20s} @ 0x{addr:x} : {mname} (ok={ok})")

    # Step past initial pause at 0x756f4ab30c
    print("\nStepping past initial pause (0x756f4ab30c)...")
    gdb.step()
    regs = gdb.get_regs()
    print(f"After step: PC=0x{regs[32]:x}, LR=0x{regs[30]:x}, SP=0x{regs[31]:x}")

    print("\nContinuing execution (observation window: up to 180s)...")
    gdb.continue_async()

    t0 = time.time()
    hits = []
    task_417 = 0xffffffc0fb178000
    task_255 = 0xffffffc0ee518000
    last_poll = t0

    while time.time() - t0 < 180.0:
        elapsed = time.time() - t0
        pkt = gdb.wait_for_stop(timeout=1.0)
        
        if pkt and (b'$T' in pkt or b'$S' in pkt):
            regs = gdb.get_regs()
            if regs and len(regs) > 32:
                pc = regs[32]
                lr = regs[30]
                sp = regs[31]
                x0 = regs[0]
                x1 = regs[1]
                x8 = regs[8]
                minfo = milestones.get(pc, None)
                mid = minfo[0] if minfo else f"UNK_0x{pc:x}"
                mname = minfo[1] if minfo else "Unknown"

                print(f"\n[{elapsed:5.2f}s] *** HIT #{len(hits)+1}: {mid:20s} @ 0x{pc:x} : {mname} ***")
                print(f"        -> X0=0x{x0:x}, X1=0x{x1:x}, X8=0x{x8:x}, LR=0x{lr:x}, SP=0x{sp:x}")
                hits.append((elapsed, mid, mname, pc, lr, sp, x0, x1, x8))

                if mid in ("M11_CALL_SWAP", "M12_PLT_EGL_SWAP", "M13_DSO_EGL_SWAP"):
                    print("\n" + "=" * 80)
                    print(f">>> CRITICAL SUCCESS: {mid} REACHED IN BOOTANIMATION! <<<")
                    print("=" * 80)
                    break

                # Step past breakpoint and rearm
                gdb.remove_bp(pc)
                gdb.step()
                gdb.set_bp(pc)
                gdb.continue_async()

        # Periodic status telemetry every 10s
        if time.time() - last_poll >= 10.0:
            last_poll = time.time()
            print(f"  [STATUS @ {elapsed:5.1f}s] Observing presentation pipeline... Milestone hits so far: {len(hits)}")

    vm_utils.qmp_hmp(s_qmp, f_qmp, "stop")
    print(f"\nObservation window concluded ({time.time()-t0:.1f}s). Total milestone hits: {len(hits)}")
    print("=" * 80)
    for el, mid, name, pc, lr, sp, x0, x1, x8 in hits:
        print(f"  [{el:5.2f}s] {mid:20s} @ 0x{pc:x} : {name}")
        print(f"          X0=0x{x0:x}, X1=0x{x1:x}, X8=0x{x8:x}, LR=0x{lr:x}, SP=0x{sp:x}")
    print("=" * 80)

    # Disarm probes
    for addr in milestones:
        gdb.remove_bp(addr)

    gdb.sock.close()
    s_qmp.close()
    print("Done.")

if __name__ == "__main__":
    main()
