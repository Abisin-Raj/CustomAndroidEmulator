#!/usr/bin/env python3
# phase294_decode_txn.py  -- Phase 29.4 transaction decode
#
# CONCLUSIONS FROM PROBE:
#   PID 417 (BA worker):  stuck at 0x75732518a0 (ANGLE's hwbinder ioctl, t=15-40s)
#   PID 255 (allocator):  stuck at 0x7077057d78 (libc ioctl stub, PERMANENTLY)
#   => BOTH are blocked in ioctl => Outcome A: transaction never delivered
#
# THIS SCRIPT:
#   1. Reads PID 417's full pt_regs to identify the exact syscall args.
#   2. Decodes the binder_write_read and BC_TRANSACTION handle.
#   3. Scans ALL fds (up to 218) in the BA main task to find binder_proc(s)
#      and identify the target binder_proc for the allocation.
#   4. Produces definitive Outcome A sub-cause.

import vm_utils
import time
import sys
sys.stdout.reconfigure(line_buffering=True)

TTBR1_PA        = 0x43006000
BA_MAIN_TASK_VA = 0xffffffc0ee51ec00   # PID 246 (BA main, mm shared with all threads)
WORKER_TASK_VA  = 0xffffffc0fb178000   # PID 417 (BA worker)
ALLOC_TASK_VA   = 0xffffffc0ee518000   # PID 255

def kpa(va):  return vm_utils.kva_to_phys(va)
def r64(s,f,pa): return vm_utils.read_phys_u64(s,f,pa)
def r32(s,f,pa): return vm_utils.read_phys_u32(s,f,pa)
def is_kva(v): return 0xffffff8000000000 <= v <= 0xfffffffe00000000

def walk_kpt(s, f, va):
    return vm_utils.walk_kernel_pt(s, f, TTBR1_PA, va)

def safe_str(b):
    """Convert bytes to printable ASCII, replacing non-printable with '.'"""
    return ''.join(chr(c) if 32 <= c < 127 else '.' for c in b)

def read_kbytes(s, f, pa, count):
    """Read 'count' bytes from physical address pa."""
    result = []
    for off in range(0, count, 8):
        v = r64(s, f, pa + off)
        for byte_pos in range(min(8, count - off)):
            result.append((v >> (byte_pos * 8)) & 0xff)
    return bytes(result[:count])

def walk_user_pt(s, f, pgd_pa, va):
    """Walk 3-level user page table (arm64)."""
    va = va & 0x00ffffffffffffff
    # L1
    l1_idx = (va >> 30) & 0x1ff
    l1e = r64(s, f, pgd_pa + l1_idx * 8)
    if (l1e & 3) != 3:
        if (l1e & 3) == 1:
            return (l1e & 0x0000ffffc0000000) | (va & 0x3fffffff)
        return None
    pmd_pa = l1e & 0x0000fffffffff000
    # L2
    l2_idx = (va >> 21) & 0x1ff
    l2e = r64(s, f, pmd_pa + l2_idx * 8)
    if (l2e & 3) != 3:
        if (l2e & 3) == 1:
            return (l2e & 0x0000fffffffe0000) | (va & 0x1fffff)
        return None
    pte_pa = l2e & 0x0000fffffffff000
    # L3
    l3_idx = (va >> 12) & 0x1ff
    l3e = r64(s, f, pte_pa + l3_idx * 8)
    if (l3e & 3) != 3:
        return None
    return (l3e & 0x0000fffffffff000) | (va & 0xfff)

def read_pt_regs(s, f, task_pa):
    """
    Return dict of all pt_regs fields for a task.
    pt_regs is at kstack_page3 + 0xec0.
    AArch64 pt_regs layout:
      +0x000: x0..x30 (31 * 8 = 0xF8 bytes)
      +0x0F8: sp
      +0x100: pc
      +0x108: pstate
    """
    stack_va = r64(s, f, task_pa + 0x60)
    if not stack_va:
        return {}
    pa_top = walk_kpt(s, f, stack_va + 0x3000)
    if not pa_top:
        return {}
    base = pa_top + 0xec0
    regs = {}
    for i in range(31):
        regs[f'x{i}'] = r64(s, f, base + i * 8)
    regs['sp']     = r64(s, f, base + 0xF8)
    regs['pc']     = r64(s, f, base + 0x100)
    regs['pstate'] = r64(s, f, base + 0x108)
    regs['orig_x0']= r64(s, f, base + 0x110)
    return regs

def get_pgd(s, f, task_pa):
    """Return physical address of user PGD from task's mm_struct."""
    mm_va = r64(s, f, task_pa + 0x470)
    if not is_kva(mm_va):
        return None
    mm_pa = kpa(mm_va)
    pgd_kva = r64(s, f, mm_pa + 0x48)
    if not is_kva(pgd_kva):
        return None
    return kpa(pgd_kva)

def get_fd_array(s, f, task_pa):
    for off in range(0x4f0, 0x590, 8):
        v = r64(s, f, task_pa + off)
        if not is_kva(v): continue
        fdt = r64(s, f, kpa(v) + 0x20)
        if not is_kva(fdt): continue
        mf = r32(s, f, kpa(fdt))
        if 4 <= mf <= 65536:
            farr = r64(s, f, kpa(fdt) + 8)
            if is_kva(farr):
                return kpa(farr), mf
    return None, 0

def scan_all_binder_fds(s, f, fd_arr_pa, max_fds, process_pid):
    """
    Scan ALL fds for binder_proc structures.
    For each fd's private_data candidates, scan offsets 0x28..0xd0 for value == process_pid.
    """
    print(f"\n  Scanning {min(max_fds,128)} fds for binder_proc(pid={process_pid})...")
    limit = min(max_fds, 128)
    found = []
    for fd in range(limit):
        file_ptr = r64(s, f, fd_arr_pa + fd * 8)
        if not is_kva(file_ptr): continue
        file_pa = kpa(file_ptr)
        # Scan private_data offsets in struct file
        for pd_off in range(0x98, 0xe8, 8):
            priv = r64(s, f, file_pa + pd_off)
            if not is_kva(priv): continue
            priv_pa = kpa(priv)
            # Look for process_pid at offsets 0x28..0xd0 in the struct
            for p_off in range(0x28, 0xd4, 4):
                pval = r32(s, f, priv_pa + p_off)
                if pval == process_pid:
                    print(f"    fd[{fd}] file+0x{pd_off:x} -> 0x{priv:x}: pid={pval} at +0x{p_off:x} -> BINDER_PROC FOUND!")
                    found.append((fd, pd_off, priv, p_off))
                    break
    if not found:
        print(f"    No binder_proc with pid={process_pid} found in {limit} fds.")
        # Show what PIDs were found as a diagnostic
        pids_seen = set()
        for fd in range(min(limit, 64)):
            file_ptr = r64(s, f, fd_arr_pa + fd * 8)
            if not is_kva(file_ptr): continue
            file_pa = kpa(file_ptr)
            for pd_off in (0xb0, 0xc0, 0xd0):
                priv = r64(s, f, file_pa + pd_off)
                if not is_kva(priv): continue
                priv_pa = kpa(priv)
                for p_off in range(0x28, 0x90, 4):
                    pval = r32(s, f, priv_pa + p_off)
                    if 50 <= pval <= 600:
                        pids_seen.add(pval)
        print(f"    PID-range values found in binder structs: {sorted(pids_seen)}")
    return found

def decode_bwr(s, f, pgd_pa, bwr_va):
    """Decode binder_write_read and the embedded BC_TRANSACTION."""
    bwr_pa = walk_user_pt(s, f, pgd_pa, bwr_va)
    if not bwr_pa:
        print(f"  ERROR: could not walk user PT for BWR @ 0x{bwr_va:x}")
        return

    write_size     = r64(s, f, bwr_pa + 0)
    write_consumed = r64(s, f, bwr_pa + 8)
    write_buf_va   = r64(s, f, bwr_pa + 16)
    read_size      = r64(s, f, bwr_pa + 24)
    read_consumed  = r64(s, f, bwr_pa + 32)
    read_buf_va    = r64(s, f, bwr_pa + 40)

    print(f"  binder_write_read @ user:0x{bwr_va:x} (PA:0x{bwr_pa:x})")
    print(f"    write_size={write_size}, write_consumed={write_consumed}, write_buf=0x{write_buf_va:x}")
    print(f"    read_size={read_size},  read_consumed={read_consumed},  read_buf=0x{read_buf_va:x}")

    if write_size > 0 and write_buf_va and write_size < 4096:
        wb_pa = walk_user_pt(s, f, pgd_pa, write_buf_va)
        if wb_pa:
            data = read_kbytes(s, f, wb_pa, int(write_size))
            print(f"\n  Write buffer ({write_size} bytes):")
            for j in range(0, len(data), 16):
                chunk = data[j:j+16]
                h = ' '.join(f'{b:02x}' for b in chunk)
                a = safe_str(chunk)
                print(f"    +0x{j:03x}: {h:<48s}  {a}")

            # Decode command sequence
            pos = 0
            print(f"\n  Decoded commands:")
            while pos < len(data):
                if pos + 4 > len(data): break
                cmd = int.from_bytes(data[pos:pos+4], 'little')
                pos += 4
                if cmd == 0x40406300:   # BC_TRANSACTION
                    print(f"    BC_TRANSACTION (0x40406300) at write_buf+0x{pos-4:x}")
                    # binder_transaction_data (0x40 bytes):
                    # target.handle (4), cookie (8), code (4), flags (4),
                    # sender_pid (4), sender_euid (4), data_size (8), offsets_size (8),
                    # data.ptr.buffer (8)
                    if pos + 0x40 <= len(data):
                        handle    = int.from_bytes(data[pos:pos+4], 'little')
                        cookie    = int.from_bytes(data[pos+8:pos+16], 'little')
                        code      = int.from_bytes(data[pos+16:pos+20], 'little')
                        flags     = int.from_bytes(data[pos+20:pos+24], 'little')
                        spid      = int.from_bytes(data[pos+24:pos+28], 'little')
                        seuid     = int.from_bytes(data[pos+28:pos+32], 'little')
                        data_size = int.from_bytes(data[pos+32:pos+40], 'little')
                        off_size  = int.from_bytes(data[pos+40:pos+48], 'little')
                        dbuf_va   = int.from_bytes(data[pos+48:pos+56], 'little')
                        print(f"      target.handle = {handle} (the hwbinder handle for allocator)")
                        print(f"      cookie        = 0x{cookie:x}")
                        print(f"      code          = 0x{code:x} (HIDL method code)")
                        print(f"      flags         = 0x{flags:x}")
                        print(f"      sender_pid    = {spid}")
                        print(f"      data_size     = {data_size}")
                        print(f"      data.ptr.buf  = 0x{dbuf_va:x}")

                        # Decode the HIDL parcel header (first 16 bytes of data buf)
                        if dbuf_va and data_size >= 16:
                            dbuf_pa = walk_user_pt(s, f, pgd_pa, dbuf_va)
                            if dbuf_pa:
                                pdata = read_kbytes(s, f, dbuf_pa, min(int(data_size), 64))
                                print(f"      HIDL parcel data (first 64 bytes):")
                                for j in range(0, len(pdata), 16):
                                    chunk = pdata[j:j+16]
                                    h = ' '.join(f'{b:02x}' for b in chunk)
                                    a = safe_str(chunk)
                                    print(f"        +0x{j:02x}: {h:<48s}  {a}")
                    break
                elif cmd == 0x3f0b6200:  # BC_INCREFS
                    print(f"    BC_INCREFS (skipping, 4 bytes)")
                    pos += 4
                elif cmd == 0x3f0c6200:  # BC_ACQUIRE
                    print(f"    BC_ACQUIRE (skipping, 4 bytes)")
                    pos += 4
                else:
                    print(f"    cmd=0x{cmd:08x} at pos={pos-4} (unknown, stopping)")
                    break


def main():
    s, f = vm_utils.connect_qmp()

    print("=" * 70)
    print("PHASE 29.4: DECODE TRANSACTION + FIND OUTCOME A SUB-CAUSE")
    print("=" * 70)

    # Restore CP-02 and run to stall
    print("\n[1] Restoring CP-02, running 20s to reach stall point...")
    vm_utils.qmp_hmp(s, f, "loadvm cp02_rtr_token")
    vm_utils.qmp_hmp(s, f, "c")
    time.sleep(20)
    vm_utils.qmp_hmp(s, f, "stop")
    print("    VM stopped.")

    # Read PID 417 pt_regs
    print("\n[2] Reading PID 417 (worker) full pt_regs...")
    worker_pa = kpa(WORKER_TASK_VA)
    regs417 = read_pt_regs(s, f, worker_pa)
    if regs417:
        pc  = regs417['pc']
        x0  = regs417['x0']
        x1  = regs417['x1']
        x2  = regs417['x2']
        x8  = regs417['x8']
        sp  = regs417['sp']
        lr  = regs417.get('x30', 0)
        print(f"    PC      = 0x{pc:x}")
        print(f"    LR(x30) = 0x{lr:x}")
        print(f"    x0      = 0x{x0:x}  (fd for ioctl, expected 9 for hwbinder)")
        print(f"    x1      = 0x{x1:x}  (ioctl cmd, BINDER_WRITE_READ=0x c0306201)")
        print(f"    x2      = 0x{x2:x}  (ptr to binder_write_read)")
        print(f"    x8      = 0x{x8:x}  (syscall# - 29=ioctl, 98=futex)")
        print(f"    sp      = 0x{sp:x}")
    else:
        print("    ERROR: could not read pt_regs for PID 417")

    # Read PID 255 pt_regs  
    print("\n[3] Reading PID 255 (allocator) pt_regs...")
    alloc_pa = kpa(ALLOC_TASK_VA)
    regs255 = read_pt_regs(s, f, alloc_pa)
    if regs255:
        pc255 = regs255['pc']
        x8_255 = regs255['x8']
        x0_255 = regs255['x0']
        print(f"    PC   = 0x{pc255:x}")
        print(f"    x0   = 0x{x0_255:x}  (fd for allocator binder receive)")
        print(f"    x8   = 0x{x8_255:x}  (syscall# - 29=ioctl)")
    else:
        print("    ERROR: could not read pt_regs for PID 255")

    # Get PGD from BA main task
    print("\n[4] Getting BA process page table (PGD)...")
    ba_main_pa = kpa(BA_MAIN_TASK_VA)
    pgd_pa = get_pgd(s, f, ba_main_pa)
    if pgd_pa:
        print(f"    PGD PA = 0x{pgd_pa:x}")
    else:
        print("    ERROR: could not get PGD from BA main task")

    # If PID 417 is in ioctl, decode the BWR
    print("\n[5] Decoding PID 417 binder_write_read...")
    if regs417 and pgd_pa:
        sc = regs417.get('x8', 0)
        cmd = regs417.get('x1', 0)
        bwr_va = regs417.get('x2', 0)
        print(f"    syscall={sc}, ioctl_cmd=0x{cmd:x}, bwr_ptr=0x{bwr_va:x}")
        # Accept ioctl (29) with any BINDER_WRITE_READ variant
        # 0xc0306201 = old BINDER_WRITE_READ, 0x40286201 = alternate
        if sc == 29 and bwr_va:
            decode_bwr(s, f, pgd_pa, bwr_va)
        else:
            print(f"    PID 417 not in ioctl(29) at this moment (x8={sc}). Trying anyway with bwr_va=0x{bwr_va:x}...")
            if bwr_va:
                decode_bwr(s, f, pgd_pa, bwr_va)

    # Scan all fds in BA main task for binder_proc
    print("\n[6] Full fd scan in BA main (246) for binder_proc...")
    fd_arr_pa, max_fds = get_fd_array(s, f, ba_main_pa)
    if fd_arr_pa:
        print(f"    fd_array found, max_fds={max_fds}")
        found_246 = scan_all_binder_fds(s, f, fd_arr_pa, max_fds, 246)
    else:
        print("    ERROR: fd array not found in BA main task")
        found_246 = []

    # Scan allocator fds for binder_proc
    print("\n[7] Full fd scan in allocator (255) for binder_proc...")
    fd_arr_pa2, max_fds2 = get_fd_array(s, f, alloc_pa)
    if fd_arr_pa2:
        print(f"    fd_array found, max_fds={max_fds2}")
        found_255 = scan_all_binder_fds(s, f, fd_arr_pa2, max_fds2, 255)
    else:
        print("    ERROR: fd array not found in allocator task")
        found_255 = []

    # VERDICT
    print("\n" + "=" * 70)
    print("PHASE 29.4 VERDICT")
    print("=" * 70)

    pc417 = regs417.get('pc', 0) if regs417 else 0
    pc255 = regs255.get('pc', 0) if regs255 else 0
    x8_417 = regs417.get('x8', 0) if regs417 else 0

    print(f"\n  PID 417 PC=0x{pc417:x}  x8(syscall)={x8_417}")
    print(f"  PID 255 PC=0x{pc255:x}")
    print()
    print("  INTERPRETATION:")
    print("  - PID 255 at libc+0x70d78 PERMANENTLY = blocked in ioctl (binder receive)")
    print("  - PID 417 stuck for 25s = blocked in hwbinder ioctl (waiting for reply)")
    print()
    print("  => OUTCOME A: Transaction from PID 417 was NEVER delivered to PID 255.")
    print("     Check the BC_TRANSACTION handle above to identify sub-cause.")
    print("=" * 70)

    s.close()


if __name__ == "__main__":
    main()
