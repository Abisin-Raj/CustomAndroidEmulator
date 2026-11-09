#!/usr/bin/env python3
"""
trace_gpu_command_payloads.py

Step 2 of Android Display & GPU Investigation:
Trace whether GPU command submissions (IOCTL_KGSL_GPU_COMMAND) contain real draw work.
Verify if the guest is submitting valid workloads or only advancing fence timestamps.

Monitors:
  - kgsl_ioctl_gpu_command (0xffffff8008b34d50)
  - a6xx_ringbuffer_addcmds (0xffffff8008b7a2d0)
  - sde_crtc_atomic_flush (0xffffff8008a4f750)
"""

import sys
import os
import time
import socket
import json
import struct
import subprocess

sys.stdout.reconfigure(encoding='utf-8', errors='replace', line_buffering=True)

SCRATCH_DIR = r"C:\Users\ABISIN RAJ\.gemini\antigravity\brain\8d07ce0d-ae42-41f7-a74f-20b2087f0ea2\scratch"
LOG_FILE    = os.path.join(SCRATCH_DIR, "gpu_command_trace.log")
OUT_JSON    = os.path.join(SCRATCH_DIR, "gpu_command_trace_result.json")

OVERLAY_IMG = r"C:\qemu_work\emmc_overlay_trace_scanout.qcow2"
KERNEL_IMG  = r"C:\qemu_work\kernel_kgsl_d8_8.img"
RAMDISK_IMG = r"C:\qemu_work\boot_ramdisk_dri.gz"
DTB_IMG     = r"C:\qemu_work\virt_hw442.dtb"
QEMU_EXE    = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidQEMU\build\qemu-system-aarch64.exe"
PC_BIOS     = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidQEMU\pc-bios"

QMP_PORT = 4499
GDB_PORT = 1999
SNAPSHOT = "cp02_sf_stable"

BP_KGSL_GPU_COMMAND = 0xffffff8008b34d50
BP_A6XX_ADDCMDS     = 0xffffff8008b7a2d0
BP_SDE_CRTC_FLUSH   = 0xffffff8008a4f750

log_f = open(LOG_FILE, "w", encoding="utf-8")
def log(msg=""):
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{timestamp}] {msg}"
    print(line, flush=True)
    log_f.write(line + "\n")
    log_f.flush()

PM4_OPCODES = {
    16: "CP_NOP",
    17: "CP_RECORD_PFP_TIMESTAMP",
    18: "CP_WAIT_MEM_WRITES",
    19: "CP_WAIT_FOR_ME",
    23: "IN_IB_PREFETCH_END",
    25: "CP_UNKNOWN_19",
    26: "CP_UNKNOWN_1A",
    28: "CP_PREEMPT_ENABLE",
    29: "CP_SKIP_IB2_ENABLE_GLOBAL",
    30: "CP_PREEMPT_TOKEN",
    31: "IN_SUBBLK_PREFETCH",
    32: "IN_INSTR_PREFETCH",
    33: "CP_REG_RMW",
    34: "CP_DRAW_INDX",
    35: "CP_VIZ_QUERY",
    36: "CP_DRAW_AUTO",
    37: "CP_SET_STATE",
    38: "CP_WAIT_FOR_IDLE",
    39: "CP_IM_LOAD",
    40: "CP_DRAW_INDIRECT",
    41: "CP_DRAW_INDX_INDIRECT",
    43: "CP_IM_LOAD_IMMEDIATE",
    44: "CP_BLIT",
    45: "CP_SET_CONSTANT",
    46: "CP_LOAD_CONSTANT_CONTEXT",
    47: "CP_SET_BIN_DATA",
    48: "CP_LOAD_STATE",
    49: "CP_RUN_OPENCL",
    50: "CP_COND_INDIRECT_BUFFER_PFD",
    51: "CP_EXEC_CS",
    52: "CP_DRAW_INDX_BIN",
    53: "CP_DRAW_INDX_2_BIN",
    54: "CP_DRAW_INDX_2",
    55: "CP_INDIRECT_BUFFER_PFD",
    56: "CP_DRAW_INDX_OFFSET",
    57: "CP_UNK_39",
    58: "CP_COND_INDIRECT_BUFFER_PFE",
    59: "CP_INVALIDATE_STATE",
    60: "CP_WAIT_REG_MEM",
    61: "CP_MEM_WRITE",
    62: "CP_REG_TO_MEM",
    63: "CP_INDIRECT_BUFFER",
    64: "CP_INTERRUPT",
    66: "CP_MEM_TO_REG",
    67: "CP_SET_DRAW_STATE",
    68: "CP_COND_EXEC",
    69: "CP_COND_WRITE",
    70: "CP_EVENT_WRITE",
    71: "IN_INSTR_MATCH",
    72: "CP_ME_INIT",
    73: "IN_CONST_PREFETCH",
    74: "CP_SET_SHADER_BASES",
    75: "CP_SET_DRAW_INIT_FLAGS",
    76: "CP_SET_BIN",
    77: "CP_SCRATCH_TO_REG",
    78: "CP_UNKNOWN_4E",
    79: "CP_MEM_WRITE_CNTR",
    80: "CP_PERFCOUNTER_ACTION",
    81: "CP_SET_BIN_SELECT",
    82: "CP_WAIT_REG_EQ",
    83: "CP_WAIT_REG_GTE",
    85: "IN_INCR_UPDT_STATE",
    86: "IN_INCR_UPDT_CONST",
    87: "IN_INCR_UPDT_INSTR",
    88: "CP_EVENT_WRITE_SHD",
    89: "CP_EVENT_WRITE_CFL",
    91: "CP_EVENT_WRITE_ZPD",
    92: "CP_CONTEXT_REG_BUNCH",
    93: "CP_WAIT_IB_PFD_COMPLETE",
    94: "CP_CONTEXT_UPDATE",
    95: "CP_SET_PROTECTED_MODE",
    100: "CP_SET_VISIBILITY_OVERRIDE",
    101: "CP_SET_MARKER",
    102: "CP_SET_SECURE_MODE",
    105: "CP_PREEMPT_ENABLE_GLOBAL",
    106: "CP_PREEMPT_ENABLE_LOCAL",
    107: "CP_CONTEXT_SWITCH_YIELD",
    108: "CP_SET_RENDER_MODE",
    110: "CP_COMPUTE_CHECKPOINT",
    111: "CP_BOOTSTRAP_UCODE",
    113: "CP_TEST_TWO_MEMS",
    115: "CP_MEM_TO_MEM",
    116: "CP_WIDE_REG_WRITE",
    120: "CP_REG_WR_NO_CTXT",
}

DRAW_OPCODES = {
    34: "CP_DRAW_INDX",
    36: "CP_DRAW_AUTO",
    40: "CP_DRAW_INDIRECT",
    41: "CP_DRAW_INDX_INDIRECT",
    44: "CP_BLIT",
    51: "CP_EXEC_CS",
    52: "CP_DRAW_INDX_BIN",
    53: "CP_DRAW_INDX_2_BIN",
    54: "CP_DRAW_INDX_2",
    56: "CP_DRAW_INDX_OFFSET",
}

class QMPClient:
    def __init__(self, host='127.0.0.1', port=QMP_PORT):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(15.0)
        self.sock.connect((host, port))
        self.rfile = self.sock.makefile('r', encoding='utf-8')
        banner = self._read_json()
        res = self.execute("qmp_capabilities")
        assert "return" in res, f"Capabilities negotiation failed: {res}"

    def _read_json(self):
        while True:
            line = self.rfile.readline()
            if not line:
                raise EOFError("QMP closed")
            line = line.strip()
            if line:
                return json.loads(line)

    def execute(self, cmd, arguments=None):
        req = {"execute": cmd}
        if arguments:
            req["arguments"] = arguments
        self.sock.sendall((json.dumps(req) + "\r\n").encode('utf-8'))
        while True:
            resp = self._read_json()
            if "event" in resp:
                continue
            return resp

    def hmp(self, cmd_line):
        res = self.execute("human-monitor-command", {"command-line": cmd_line})
        return res.get("return", "")

    def close(self):
        try: self.rfile.close()
        except: pass
        try: self.sock.close()
        except: pass

class GDBClient:
    def __init__(self, port=GDB_PORT, timeout=10.0):
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

    def read_u32(self, addr):
        b = self.read_mem(addr, 4)
        if len(b) == 4: return struct.unpack('<I', b)[0]
        return None

    def read_u64(self, addr):
        b = self.read_mem(addr, 8)
        if len(b) == 8: return struct.unpack('<Q', b)[0]
        return None

    def close(self):
        try: self.sock.close()
        except: pass

def parse_pm4_stream(data):
    """Parses Adreno PM4 packet stream."""
    packets = []
    num_dwords = len(data) // 4
    if num_dwords == 0: return packets

    dwords = struct.unpack(f'<{num_dwords}I', data[:num_dwords*4])
    idx = 0
    while idx < num_dwords:
        hdr = dwords[idx]
        pkt_type = (hdr >> 28) & 0xF

        if pkt_type == 7: # Type 7 packet
            cnt = hdr & 0x7F
            op = (hdr >> 16) & 0x7F
            op_name = PM4_OPCODES.get(op, f"UNK_TYPE7_0x{op:02x}")
            is_draw = op in DRAW_OPCODES
            payload = dwords[idx+1 : idx+1+cnt] if idx+1+cnt <= num_dwords else []
            packets.append({
                "type": 7,
                "opcode": op,
                "name": op_name,
                "count": cnt,
                "is_draw": is_draw,
                "payload_hex": [f"0x{w:08x}" for w in payload[:8]]
            })
            idx += 1 + cnt

        elif pkt_type == 4: # Type 4 packet
            cnt = hdr & 0x7F
            reg = (hdr >> 8) & 0x3FFFF
            payload = dwords[idx+1 : idx+1+cnt] if idx+1+cnt <= num_dwords else []
            packets.append({
                "type": 4,
                "opcode": None,
                "name": f"PKT4_REG_0x{reg:05x}",
                "count": cnt,
                "is_draw": False,
                "payload_hex": [f"0x{w:08x}" for w in payload[:8]]
            })
            idx += 1 + cnt

        elif (hdr & 0xC0000000) == 0xC0000000: # Type 3 packet
            op = (hdr >> 8) & 0xFF
            cnt = (hdr >> 16) & 0x3FFF
            op_name = PM4_OPCODES.get(op, f"UNK_TYPE3_0x{op:02x}")
            is_draw = op in DRAW_OPCODES
            payload = dwords[idx+1 : idx+1+cnt] if idx+1+cnt <= num_dwords else []
            packets.append({
                "type": 3,
                "opcode": op,
                "name": op_name,
                "count": cnt,
                "is_draw": is_draw,
                "payload_hex": [f"0x{w:08x}" for w in payload[:8]]
            })
            idx += 1 + cnt
        else:
            idx += 1

    return packets

def get_task_info(gdb, sp):
    """Derives task struct comm and pid from thread_info/stack."""
    stack_base = sp & ~(16384 - 1)
    task_ptr = None
    for off in (16, 8, 24, 0):
        t = gdb.read_u64(stack_base + off)
        if t and 0xffffffc000000000 <= t <= 0xffffffcfffffffff:
            task_ptr = t
            break

    comm = "unknown"
    pid = 0
    if task_ptr:
        tmem = gdb.read_mem(task_ptr, 0x700)
        if tmem:
            for off in range(0x5f0, 0x6c0):
                if off + 16 > len(tmem): break
                chunk = tmem[off:off+16]
                z = chunk.find(b'\x00')
                s = chunk[:z] if z >= 0 else chunk
                try:
                    s_str = s.decode('ascii')
                    if 3 <= len(s_str) <= 15 and all(c.isprintable() for c in s_str):
                        comm = s_str
                        break
                except: pass
            for poff in (0x498, 0x4a0, 0x518, 0x4f0, 0x4b0):
                if poff + 4 <= len(tmem):
                    p = struct.unpack('<I', tmem[poff:poff+4])[0]
                    if 1 <= p <= 65535:
                        pid = p
                        break
    return comm, pid, task_ptr

def main():
    log("=" * 80)
    log("STEP 2: ADRENO GPU COMMAND SUBMISSION & WORKLOAD TRACING")
    log("Target: IOCTL_KGSL_GPU_COMMAND (0xc040094a) & SDE Flush Correlation")
    log("=" * 80)

    # 1. Clean up prior instances
    subprocess.run(["powershell", "-Command", "Get-Process qemu-system-aarch64 -ErrorAction SilentlyContinue | Stop-Process -Force"], capture_output=True)
    time.sleep(1.0)

    # 2. Launch QEMU suspended (-S)
    qemu_cmd = [
        QEMU_EXE,
        "-L", PC_BIOS,
        "-M", "virt,gic-version=3",
        "-cpu", "cortex-a55", "-smp", "1", "-m", "4096",
        "-device", "ramfb",
        "-kernel", KERNEL_IMG,
        "-initrd", RAMDISK_IMG,
        "-dtb", DTB_IMG,
        "-drive", f"file={OVERLAY_IMG},if=sd,format=qcow2,id=emmc0",
        "-display", "none",
        "-qmp", f"tcp:127.0.0.1:{QMP_PORT},server=on,wait=off",
        "-gdb", f"tcp::{GDB_PORT}",
        "-S"
    ]

    log("[1] Launching QEMU in suspended state...")
    proc = subprocess.Popen(qemu_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(2.0)

    try:
        log("[2] Connecting to QMP...")
        qmp = QMPClient(port=QMP_PORT)
        log("    QMP connected successfully.")

        log(f"[3] Restoring VM checkpoint '{SNAPSHOT}'...")
        res_load = qmp.hmp(f"loadvm {SNAPSHOT}")
        log(f"    Checkpoint loadvm result: '{res_load.strip()}'")

        log("[4] Connecting to GDB stub...")
        gdb = GDBClient(port=GDB_PORT)
        log("    GDB stub connected successfully.")

        # Arm only critical breakpoints to allow guest to run at full virtual speed
        bps = [
            (BP_KGSL_GPU_COMMAND, "kgsl_ioctl_gpu_command"),
            (BP_A6XX_ADDCMDS, "a6xx_ringbuffer_addcmds"),
            (BP_SDE_CRTC_FLUSH, "sde_crtc_atomic_flush"),
        ]

        for addr, name in bps:
            ok = gdb.set_bp(addr)
            log(f"[*] Armed breakpoint {name} @ 0x{addr:016x}: {'SUCCESS' if ok else 'FAILED'}")

        log("[5] Resuming VM execution and monitoring event loop (up to 45s)...")
        gdb.cont_async()

        start_time = time.time()
        timeout = 45.0
        hit_counts = {name: 0 for _, name in bps}
        gpu_commands_captured = []
        sde_flushes_captured = []

        while time.time() - start_time < timeout:
            stop_resp = gdb.wait_stop(timeout=2.0)
            if not stop_resp:
                continue

            regs = gdb.get_regs()
            if not regs or len(regs) < 33:
                gdb.cont_async()
                continue

            pc = regs[32]
            sp = regs[31]
            elapsed = time.time() - start_time

            # -------------------------------------------------------------
            # Event 1: kgsl_ioctl_gpu_command (0xffffff8008b34d50)
            # -------------------------------------------------------------
            if pc == BP_KGSL_GPU_COMMAND:
                hit_counts["kgsl_ioctl_gpu_command"] += 1
                hit_idx = hit_counts["kgsl_ioctl_gpu_command"]
                comm, pid, task = get_task_info(gdb, sp)

                cmd_req_ptr = regs[2]
                cmd_mem = gdb.read_mem(cmd_req_ptr, 64)

                cmd_info = {
                    "hit_index": hit_idx,
                    "elapsed_sec": round(elapsed, 3),
                    "comm": comm,
                    "pid": pid,
                    "cmd_req_ptr": f"0x{cmd_req_ptr:016x}",
                    "numcmds": 0,
                    "numobjs": 0,
                    "numsyncs": 0,
                    "context_id": 0,
                    "timestamp": 0,
                    "commands": [],
                    "objects": [],
                    "draw_calls_count": 0,
                    "state_packets_count": 0,
                    "nop_packets_count": 0,
                }

                if len(cmd_mem) >= 64:
                    flags, cmdlist, cmdsize, numcmds, objlist, objsize, numobjs, synclist, syncsize, numsyncs, ctxt_id, ts = \
                        struct.unpack('<QQIIQIIQIIII', cmd_mem[:64])

                    cmd_info.update({
                        "flags": f"0x{flags:x}",
                        "cmdlist": f"0x{cmdlist:016x}",
                        "cmdsize": cmdsize,
                        "numcmds": numcmds,
                        "objlist": f"0x{objlist:016x}",
                        "objsize": objsize,
                        "numobjs": numobjs,
                        "numsyncs": numsyncs,
                        "context_id": ctxt_id,
                        "timestamp": ts,
                    })

                    log(f"\n>>> [GPU_COMMAND #{hit_idx} @ {elapsed:5.2f}s] Caller: '{comm}' (PID {pid}) | Ctxt: {ctxt_id} | TS: {ts} <<<")
                    log(f"    flags=0x{flags:x} numcmds={numcmds} (cmdlist=0x{cmdlist:x}) numobjs={numobjs} (objlist=0x{objlist:x}) numsyncs={numsyncs}")

                    # Read Command Objects (IBs)
                    if cmdlist and numcmds > 0 and numcmds < 64:
                        for ci in range(numcmds):
                            co_ptr = cmdlist + ci * 32
                            co_mem = gdb.read_mem(co_ptr, 32)
                            if len(co_mem) >= 32:
                                co_off, co_gpuaddr, co_size, co_flags, co_id = struct.unpack('<QQQII', co_mem)
                                log(f"    -> CMD[{ci}]: id={co_id} flags=0x{co_flags:x} gpuaddr=0x{co_gpuaddr:x} off=0x{co_off:x} size={co_size}B")

                                ib_data = gdb.read_mem(co_gpuaddr, min(co_size, 4096))
                                if not ib_data and co_off != 0:
                                    ib_data = gdb.read_mem(co_gpuaddr + co_off, min(co_size, 4096))

                                parsed = parse_pm4_stream(ib_data) if ib_data else []
                                draw_cnt = sum(1 for p in parsed if p.get("is_draw"))
                                state_cnt = sum(1 for p in parsed if not p.get("is_draw") and "NOP" not in p.get("name", ""))
                                nop_cnt = sum(1 for p in parsed if "NOP" in p.get("name", ""))

                                log(f"       PM4 Packets ({len(parsed)}): Draw={draw_cnt}, State={state_cnt}, NOP={nop_cnt}")
                                for p in parsed[:8]:
                                    log(f"         [{p['type']}] {p['name']} (cnt={p['count']}) -> {p['payload_hex']}")

                                cmd_info["draw_calls_count"] += draw_cnt
                                cmd_info["state_packets_count"] += state_cnt
                                cmd_info["nop_packets_count"] += nop_cnt

                                cmd_info["commands"].append({
                                    "index": ci,
                                    "id": co_id,
                                    "flags": f"0x{co_flags:x}",
                                    "gpuaddr": f"0x{co_gpuaddr:016x}",
                                    "size": co_size,
                                    "bytes_read": len(ib_data),
                                    "parsed_packets_count": len(parsed),
                                    "draw_calls": draw_cnt,
                                    "state_packets": state_cnt,
                                    "nop_packets": nop_cnt,
                                    "first_packets": parsed[:10]
                                })

                    # Read Referenced Memory Objects
                    if objlist and numobjs > 0 and numobjs < 64:
                        for oi in range(numobjs):
                            obj_ptr = objlist + oi * 32
                            obj_mem = gdb.read_mem(obj_ptr, 32)
                            if len(obj_mem) >= 32:
                                o_off, o_gpuaddr, o_size, o_flags, o_id = struct.unpack('<QQQII', obj_mem)
                                cmd_info["objects"].append({
                                    "index": oi,
                                    "id": o_id,
                                    "flags": f"0x{o_flags:x}",
                                    "gpuaddr": f"0x{o_gpuaddr:016x}",
                                    "offset": o_off,
                                    "size": o_size,
                                })
                                log(f"    -> OBJ[{oi}]: id={o_id} flags=0x{o_flags:x} gpuaddr=0x{o_gpuaddr:x} size={o_size}B")

                gpu_commands_captured.append(cmd_info)

            # -------------------------------------------------------------
            # Event 2: a6xx_ringbuffer_addcmds (0xffffff8008b7a2d0)
            # -------------------------------------------------------------
            elif pc == BP_A6XX_ADDCMDS:
                hit_counts["a6xx_ringbuffer_addcmds"] += 1
                comm, pid, _ = get_task_info(gdb, sp)
                log(f"[{elapsed:5.2f}s] [A6XX_ADDCMDS #{hit_counts['a6xx_ringbuffer_addcmds']}] Ringbuffer Submission | Caller: '{comm}' (PID {pid}) | numobjs={regs[3]}")

            # -------------------------------------------------------------
            # Event 3: sde_crtc_atomic_flush (0xffffff8008a4f750)
            # -------------------------------------------------------------
            elif pc == BP_SDE_CRTC_FLUSH:
                hit_counts["sde_crtc_atomic_flush"] += 1
                hit_idx = hit_counts["sde_crtc_atomic_flush"]
                comm, pid, _ = get_task_info(gdb, sp)
                crtc = regs[0]
                log(f"[{elapsed:5.2f}s] [SDE_FLUSH #{hit_idx}] Display Commit | CRTC=0x{crtc:016x} Caller: '{comm}' (PID {pid}) | Correlated GPU cmds: {len(gpu_commands_captured)}")
                sde_flushes_captured.append({
                    "hit_index": hit_idx,
                    "elapsed_sec": round(elapsed, 3),
                    "comm": comm,
                    "pid": pid,
                    "crtc": f"0x{crtc:016x}",
                    "correlated_gpu_cmds": len(gpu_commands_captured)
                })

                if hit_idx >= 6:
                    log(f"[*] Reached target of 6 display commits. Concluding trace capture.")
                    break

            # Resume execution
            gdb.cont_async()

        total_elapsed = time.time() - start_time
        log("\n" + "=" * 80)
        log("TRACE CAPTURE SUMMARY")
        log("=" * 80)
        log(f"Elapsed Time:                  {total_elapsed:.2f}s")
        log(f"Total SDE Display Flushes:     {hit_counts['sde_crtc_atomic_flush']}")
        log(f"Total GPU Command Submissions: {hit_counts['kgsl_ioctl_gpu_command']}")
        log(f"Total A6xx Ringbuffer Submits: {hit_counts['a6xx_ringbuffer_addcmds']}")
        log("=" * 80)

        out_summary = {
            "elapsed_seconds": round(total_elapsed, 3),
            "hit_counts": hit_counts,
            "gpu_commands_captured": gpu_commands_captured,
            "sde_flushes_captured": sde_flushes_captured,
        }

        with open(OUT_JSON, "w", encoding="utf-8") as f:
            json.dump(out_summary, f, indent=2)
        log(f"[+] Detailed trace saved to: {OUT_JSON}")

    finally:
        log("[*] Cleaning up QEMU session...")
        try: gdb.close()
        except: pass
        try: qmp.close()
        except: pass
        subprocess.run(["powershell", "-Command", "Get-Process qemu-system-aarch64 -ErrorAction SilentlyContinue | Stop-Process -Force"], capture_output=True)
        log("[*] QEMU terminated.")

if __name__ == "__main__":
    main()
