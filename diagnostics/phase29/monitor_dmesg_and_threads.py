# monitor_dmesg_and_threads.py
# Fast live dmesg entries and BootAnimation thread progress monitor from CP-02
import vm_utils
import struct
import time
import sys

sys.stdout.reconfigure(line_buffering=True)

KNOWN_VMAS = [
    (0x756f4aa000, 0x756f4b5000, "libbootanimation.so"),
    (0x75658d6000, 0x756597c000, "libgui.so"),
    (0x7562f99000, 0x7562fb1000, "libEGL.so"),
    (0x757319e000, 0x75732bb000, "libc.so"),
    (0x7577d13000, 0x7577e02000, "linker64"),
    (0x72d0f50000, 0x72d0f6c000, "stack:pid417"),
    (0x7fcf99e000, 0x7fcf9bf000, "stack:pid246"),
    (0x757608c000, 0x7576094000, "stack:pid428"),
]

def fast_resolve(va):
    for s_va, e_va, name in KNOWN_VMAS:
        if s_va <= va < e_va:
            return f"{name}+0x{va-s_va:x}"
    return f"0x{va:x}"

def parse_dmesg(s, f, max_bytes=65536):
    pa = vm_utils.kva_to_phys(0xffffffc0fff6f000)
    buf = bytearray()
    chunk_size = 4096
    for offset in range(0, max_bytes, chunk_size):
        res = vm_utils.qmp_hmp(s, f, f'xp /{chunk_size//8}gx 0x{pa + offset:x}')
        for l in res.splitlines():
            if ':' not in l: continue
            for word_str in l.split(':')[1].strip().split():
                try:
                    val = int(word_str, 16)
                    buf.extend(struct.pack('<Q', val))
                except: pass

    pos = 0
    entries = []
    while pos + 16 < len(buf):
        ts, total_len, text_len, dict_len, fac, lvl = struct.unpack('<QHHHBB', buf[pos:pos+16])
        if total_len == 0 or total_len > 4096:
            pos += 4
            continue
        if pos + 16 + text_len <= len(buf) and text_len > 0:
            try:
                text = buf[pos+16 : pos+16+text_len].decode('utf-8', errors='ignore')
                ts_sec = ts / 1e9
                if 0 <= ts_sec < 10000 and len(text) > 1 and text.isprintable():
                    entries.append((ts_sec, text))
                    pos += total_len
                    continue
            except: pass
        pos += 4

    entries.sort(key=lambda x: x[0])
    return entries

def get_thread_info(s, f, task_va):
    t_pa = vm_utils.kva_to_phys(task_va)
    pid = vm_utils.read_phys_u32(s, f, t_pa + 0x518)
    tgid = vm_utils.read_phys_u32(s, f, t_pa + 0x51c)
    stack_va = vm_utils.read_phys_u64(s, f, t_pa + 0x60)
    k_pc = vm_utils.read_phys_u64(s, f, t_pa + 0xa00)
    k_fp = vm_utils.read_phys_u64(s, f, t_pa + 0x9f0)
    u_pc, u_sp = 0, 0
    if stack_va:
        pa_top = vm_utils.walk_kernel_pt(s, f, 0x43006000, stack_va + 0x3000)
        if pa_top:
            res_regs = vm_utils.qmp_hmp(s, f, f"xp /34gx 0x{pa_top + 0xeb0:x}")
            regs = []
            for l in res_regs.splitlines():
                if ':' not in l: continue
                for p in l.split(':')[1].strip().split():
                    try: regs.append(int(p, 16))
                    except: pass
            if len(regs) >= 33:
                u_pc = regs[32]
                u_sp = regs[31]
    return pid, tgid, k_pc, k_fp, u_pc, u_sp

def main():
    import ensure_qemu
    ensure_qemu.ensure_qemu_running()
    s, f = vm_utils.connect_qmp()
    print("=" * 80)
    print("=== LIVE DMESG & THREAD MONITOR (CP-02 -> 40s) ===")
    print("=" * 80)

    print("Restoring CP-02 checkpoint...")
    vm_utils.qmp_hmp(s, f, "loadvm cp02_rtr_token")

    # Baseline dmesg
    init_entries = parse_dmesg(s, f)
    init_ts_max = max(e[0] for e in init_entries) if init_entries else 0
    print(f"CP-02 Baseline dmesg: {len(init_entries)} entries, latest ts = {init_ts_max:.4f}s")

    # Resume execution
    print("\nResuming VM execution...")
    vm_utils.qmp_hmp(s, f, "c")
    t_start = time.time()

    seen_ts = init_ts_max

    for step in range(1, 9): # 8 steps * 5s = 40s
        time.sleep(5.0)
        vm_utils.qmp_hmp(s, f, "stop")
        elapsed = time.time() - t_start

        # Check PID 417 state
        pid, tgid, k_pc, k_fp, u_pc, u_sp = get_thread_info(s, f, 0xffffffc0fb178000)
        u_pc_res = fast_resolve(u_pc) if u_pc else "0x0"

        # Check CPU#0
        regs_info = vm_utils.qmp_hmp(s, f, "info registers")
        cpu_pc = ""
        for l in regs_info.splitlines():
            if "PC=" in l:
                cpu_pc = l.split()[0]
                break

        print(f"\n[+{elapsed:4.1f}s] Task@0xffffffc0fb178000: PID={pid}, TGID={tgid}, u_PC={u_pc_res}, u_SP=0x{u_sp:x} | CPU#0 {cpu_pc}")

        # Check for new dmesg entries
        new_entries = parse_dmesg(s, f)
        added = [e for e in new_entries if e[0] > seen_ts]
        if added:
            print(f"  --- New Kernel Messages ({len(added)}) ---")
            for ts, text in added:
                print(f"    [{ts:10.4f}] {text}")
                seen_ts = max(seen_ts, ts)

        vm_utils.qmp_hmp(s, f, "c")

    vm_utils.qmp_hmp(s, f, "stop")
    s.close()
    print("\nObservation complete.")

if __name__ == "__main__":
    main()
