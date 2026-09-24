#!/usr/bin/env python3
# phase294_pid434_txn.py  -- Decode PID 434's hwbinder transaction
#
# PID 434 (task @ 0xffffffc0f1bc8d80) is the ACTUAL BLOCKER:
#   - It is in ioctl(fd=9, BINDER_WRITE_READ) on /dev/hwbinder
#   - PID 417/428/429 are waiting on condvars for PID 434 to complete
#   - PID 255 (allocator) is in binder receive, never gets the transaction
#
# Goal: Decode PID 434's BC_TRANSACTION to find:
#   - Target handle (which binder service is being called)
#   - HIDL method code
#   - Why PID 255 never receives it

import vm_utils
import time
import sys
sys.stdout.reconfigure(line_buffering=True)

TTBR1_PA        = 0x43006000
BA_MAIN_TASK_VA = 0xffffffc0ee51ec00   # PID 246 (BA main, mm + pgd)
PID434_TASK_VA  = 0xffffffc0f1bc8d80   # PID 434 (actual hwbinder sender!)
ALLOC_TASK_VA   = 0xffffffc0ee518000   # PID 255 (allocator)

def kpa(va):  return vm_utils.kva_to_phys(va)
def r64(s,f,pa): return vm_utils.read_phys_u64(s,f,pa)
def r32(s,f,pa): return vm_utils.read_phys_u32(s,f,pa)
def r8(s,f,pa):  return r32(s,f,pa) & 0xff
def is_kva(v): return 0xffffff8000000000 <= v <= 0xfffffffe00000000

def walk_kpt(s, f, va):
    return vm_utils.walk_kernel_pt(s, f, TTBR1_PA, va)

def walk_user_pt(s, f, pgd_pa, va):
    va = va & 0x00ffffffffffffff
    l1_idx = (va >> 30) & 0x1ff
    l1e = r64(s, f, pgd_pa + l1_idx * 8)
    if (l1e & 3) != 3:
        if (l1e & 3) == 1:
            return (l1e & 0x0000ffffc0000000) | (va & 0x3fffffff)
        return None
    pmd_pa = l1e & 0x0000fffffffff000
    l2_idx = (va >> 21) & 0x1ff
    l2e = r64(s, f, pmd_pa + l2_idx * 8)
    if (l2e & 3) != 3:
        if (l2e & 3) == 1:
            return (l2e & 0x0000fffffffe0000) | (va & 0x1fffff)
        return None
    pte_pa = l2e & 0x0000fffffffff000
    l3_idx = (va >> 12) & 0x1ff
    l3e = r64(s, f, pte_pa + l3_idx * 8)
    if (l3e & 3) != 3:
        return None
    return (l3e & 0x0000fffffffff000) | (va & 0xfff)

def read_pt_regs(s, f, task_pa):
    sv = r64(s, f, task_pa + 0x60)
    if not sv: return {}
    pa = walk_kpt(s, f, sv + 0x3000)
    if not pa: return {}
    base = pa + 0xec0
    regs = {}
    for i in range(31):
        regs[f'x{i}'] = r64(s, f, base + i * 8)
    regs['sp']  = r64(s, f, base + 0xF8)
    regs['pc']  = r64(s, f, base + 0x100)
    regs['pstate'] = r64(s, f, base + 0x108)
    return regs

def get_pgd(s, f, task_pa):
    mm_va = r64(s, f, task_pa + 0x470)
    if not is_kva(mm_va): return None
    mm_pa = kpa(mm_va)
    pgd_kva = r64(s, f, mm_pa + 0x48)
    if not is_kva(pgd_kva): return None
    return kpa(pgd_kva)

def safe_str(data):
    return ''.join(chr(b) if 32 <= b < 127 else '.' for b in data)

def read_user_bytes(s, f, pgd_pa, va, count):
    result = []
    count = min(count, 4096)
    off = 0
    while off < count:
        pa = walk_user_pt(s, f, pgd_pa, va + off)
        if pa is None:
            result.extend([0] * min(8, count - off))
            off += 8
            continue
        # Read up to the end of this page
        page_remaining = 0x1000 - (pa & 0xfff)
        chunk = min(page_remaining, count - off, 512)
        chunk = (chunk + 7) & ~7  # align to 8
        for i in range(0, chunk, 8):
            v = r64(s, f, pa + i)
            for b in range(min(8, count - off - i)):
                result.append((v >> (b * 8)) & 0xff)
        off += chunk
    return bytes(result[:count])

def hexdump(data, base_addr=0, indent="  "):
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        h = ' '.join(f'{b:02x}' for b in chunk)
        a = safe_str(chunk)
        print(f"{indent}+0x{i:03x} ({base_addr+i:#x}): {h:<48s}  {a}")

def decode_binder_write_read(s, f, pgd_pa, bwr_va, label="BWR"):
    bwr_pa = walk_user_pt(s, f, pgd_pa, bwr_va)
    if not bwr_pa:
        print(f"  ERROR: Cannot walk user PT for {label} @ 0x{bwr_va:x}")
        return None

    ws  = r64(s, f, bwr_pa + 0)
    wc  = r64(s, f, bwr_pa + 8)
    wb  = r64(s, f, bwr_pa + 16)
    rs  = r64(s, f, bwr_pa + 24)
    rc  = r64(s, f, bwr_pa + 32)
    rb  = r64(s, f, bwr_pa + 40)

    print(f"\n  {label} @ 0x{bwr_va:x} (PA 0x{bwr_pa:x}):")
    print(f"    write_size     = {ws}")
    print(f"    write_consumed = {wc}")
    print(f"    write_buffer   = 0x{wb:x}")
    print(f"    read_size      = {rs}")
    print(f"    read_consumed  = {rc}")
    print(f"    read_buffer    = 0x{rb:x}")

    if ws > 0 and ws <= 4096 and wb:
        data = read_user_bytes(s, f, pgd_pa, wb, int(ws))
        print(f"\n  Write buffer ({ws} bytes):")
        hexdump(data, wb)

        # Parse binder commands
        print(f"\n  Command parse:")
        pos = 0
        while pos + 4 <= len(data):
            cmd = int.from_bytes(data[pos:pos+4], 'little')
            pos += 4
            if cmd == 0x40406300:  # BC_TRANSACTION
                print(f"    BC_TRANSACTION at wb+0x{pos-4:x}")
                if pos + 64 <= len(data):
                    # binder_transaction_data layout:
                    target_handle = int.from_bytes(data[pos:pos+4], 'little')
                    cookie        = int.from_bytes(data[pos+8:pos+16], 'little')
                    code          = int.from_bytes(data[pos+16:pos+20], 'little')
                    flags         = int.from_bytes(data[pos+20:pos+24], 'little')
                    sender_pid    = int.from_bytes(data[pos+24:pos+28], 'little')
                    sender_euid   = int.from_bytes(data[pos+28:pos+32], 'little')
                    data_size     = int.from_bytes(data[pos+32:pos+40], 'little')
                    off_size      = int.from_bytes(data[pos+40:pos+48], 'little')
                    data_buf_va   = int.from_bytes(data[pos+48:pos+56], 'little')
                    off_buf_va    = int.from_bytes(data[pos+56:pos+64], 'little')

                    print(f"      target.handle  = {target_handle}  <- binder handle for target service")
                    print(f"      code           = 0x{code:x}  <- HIDL method index")
                    print(f"      flags          = 0x{flags:x}")
                    print(f"      sender_pid     = {sender_pid}")
                    print(f"      data_size      = {data_size}")
                    print(f"      data.ptr.buf   = 0x{data_buf_va:x}")
                    print(f"      data.ptr.offsets = 0x{off_buf_va:x}")

                    # Decode interface token from parcel data
                    if data_buf_va and data_size > 0:
                        pdata = read_user_bytes(s, f, pgd_pa, data_buf_va, min(int(data_size), 128))
                        print(f"\n      HIDL parcel data (first 128 bytes):")
                        hexdump(pdata, data_buf_va, "        ")
                        # Try to extract UTF-16 interface name
                        # In HIDL, the first field after the StrictMode header is the interface name
                        # Format: int32 strict_mode, int32 name_len, UTF-16 chars
                        if len(pdata) >= 8:
                            sm = int.from_bytes(pdata[0:4], 'little')
                            name_len = int.from_bytes(pdata[4:8], 'little')
                            print(f"\n      StrictMode = 0x{sm:x}")
                            if 0 < name_len <= 64:
                                chars = []
                                for i in range(name_len):
                                    off2 = 8 + i * 2
                                    if off2 + 2 <= len(pdata):
                                        ch = int.from_bytes(pdata[off2:off2+2], 'little')
                                        if 32 <= ch < 127:
                                            chars.append(chr(ch))
                                print(f"      Interface name ({name_len} chars): {''.join(chars)}")
                    break  # Only parse first transaction
                pos += 64
            elif cmd == 0x3f0b6200:  # BC_INCREFS
                print(f"    BC_INCREFS handle={int.from_bytes(data[pos:pos+4],'little')}")
                pos += 4
            elif cmd == 0x3f0c6200:  # BC_ACQUIRE
                print(f"    BC_ACQUIRE handle={int.from_bytes(data[pos:pos+4],'little')}")
                pos += 4
            elif cmd == 0x3f0d6200:  # BC_RELEASE
                print(f"    BC_RELEASE handle={int.from_bytes(data[pos:pos+4],'little')}")
                pos += 4
            else:
                print(f"    Unknown cmd 0x{cmd:08x} at pos {pos-4}, stopping.")
                break
    return {'write_buffer': wb, 'write_size': ws}

def find_binder_proc_wide(s, f, task_pa, target_pid):
    """
    Wide scan: find files_struct in task and scan all fds for binder_proc.
    Returns (binder_proc_va, fd_idx, pid_offset) or (0, -1, -1).
    """
    # Find fd array
    fd_arr_pa = None
    max_fds = 0
    for off in range(0x4f0, 0x5a0, 8):
        v = r64(s, f, task_pa + off)
        if not is_kva(v): continue
        fdt = r64(s, f, kpa(v) + 0x20)
        if not is_kva(fdt): continue
        mf = r32(s, f, kpa(fdt))
        if 4 <= mf <= 65536:
            farr = r64(s, f, kpa(fdt) + 8)
            if is_kva(farr):
                fd_arr_pa = kpa(farr)
                max_fds = mf
                print(f"    files_struct @ 0x{v:x} (task+0x{off:x}), max_fds={mf}")
                break

    if not fd_arr_pa:
        return 0, -1, -1

    # Scan all fds for binder_proc with pid matching target_pid
    limit = min(max_fds, 64)
    for fd in range(limit):
        fp = r64(s, f, fd_arr_pa + fd * 8)
        if not is_kva(fp): continue
        fp_pa = kpa(fp)
        # Try private_data at various offsets
        for pd_off in range(0x98, 0xe8, 8):
            priv = r64(s, f, fp_pa + pd_off)
            if not is_kva(priv): continue
            priv_pa = kpa(priv)
            # Scan for target_pid in candidate binder_proc
            for p_off in range(0x28, 0xd4, 4):
                pval = r32(s, f, priv_pa + p_off)
                if pval == target_pid:
                    return priv, fd, p_off
    return 0, -1, -1

def inspect_binder_thread_txn(s, f, bproc_va, tid, pid_offset):
    """
    Walk binder_proc->threads RB-tree to find binder_thread for tid,
    then read its transaction_stack.
    """
    bproc_pa = kpa(bproc_va)
    rb_root_pa = bproc_pa + 0x10  # proc->threads at +0x10
    root_node = r64(s, f, rb_root_pa)
    if not is_kva(root_node):
        print(f"    binder_proc threads RB-tree empty (root=0x{root_node:x})")
        return

    stack = [root_node]
    visited = set()
    while stack and len(visited) < 64:
        node_va = stack.pop()
        if node_va in visited or not is_kva(node_va): continue
        visited.add(node_va)
        node_pa = kpa(node_va)
        rb_right = r64(s, f, node_pa + 8)
        rb_left  = r64(s, f, node_pa + 16)
        # binder_thread.rb_node at +0x08 → thread = node - 0x08
        thread_va = node_va - 0x08
        thread_pa = kpa(thread_va)
        thread_pid = r32(s, f, thread_pa + 0x20)
        txn_stack  = r64(s, f, thread_pa + 0x28)
        waiting    = r32(s, f, thread_pa + 0x40)  # looper state
        print(f"      binder_thread pid={thread_pid}, va=0x{thread_va:x}, "
              f"txn_stack=0x{txn_stack:x}, looper=0x{waiting:x}")
        if is_kva(txn_stack):
            # Decode transaction
            tx_pa = kpa(txn_stack)
            dbg_id  = r32(s, f, tx_pa + 0x00)
            from_t  = r64(s, f, tx_pa + 0x18)
            to_proc = r64(s, f, tx_pa + 0x28)
            to_th   = r64(s, f, tx_pa + 0x30)
            code    = r32(s, f, tx_pa + 0x40)
            flags   = r32(s, f, tx_pa + 0x44)
            print(f"        LIVE TRANSACTION: id={dbg_id}")
            print(f"          from_thread=0x{from_t:x}")
            print(f"          to_proc=0x{to_proc:x}")
            print(f"          to_thread=0x{to_th:x} {'(unassigned)' if to_th==0 else ''}")
            print(f"          code=0x{code:x}, flags=0x{flags:x}")
        if is_kva(rb_right): stack.append(rb_right)
        if is_kva(rb_left):  stack.append(rb_left)


def main():
    s, f = vm_utils.connect_qmp()

    print("=" * 70)
    print("PHASE 29.4: PID 434 HWBINDER TRANSACTION DECODE")
    print("PID 434 is the ACTUAL sender; PID 417 is just waiting on condvar.")
    print("=" * 70)

    # Restore and run 20s
    print("\n[1] Restoring CP-02, running 20s to stall point...")
    vm_utils.qmp_hmp(s, f, "loadvm cp02_rtr_token")
    vm_utils.qmp_hmp(s, f, "c")
    time.sleep(20)
    vm_utils.qmp_hmp(s, f, "stop")
    print("    VM stopped.")

    # Get PGD from BA main task
    ba_main_pa = kpa(BA_MAIN_TASK_VA)
    pgd_pa = get_pgd(s, f, ba_main_pa)
    print(f"    BA PGD = 0x{pgd_pa:x}" if pgd_pa else "    ERROR: no PGD")

    # Read PID 434 pt_regs
    print("\n[2] Reading PID 434 pt_regs...")
    p434_pa = kpa(PID434_TASK_VA)
    regs434 = read_pt_regs(s, f, p434_pa)
    if regs434:
        pc   = regs434['pc']
        lr   = regs434.get('x30', 0)
        x0   = regs434['x0']   # fd
        x1   = regs434['x1']   # ioctl cmd
        x2   = regs434['x2']   # BWR ptr
        x8   = regs434['x8']   # syscall#
        sp   = regs434['sp']
        print(f"    PC      = 0x{pc:x}")
        print(f"    LR(x30) = 0x{lr:x}  (return addr from ioctl)")
        print(f"    x0(fd)  = 0x{x0:x}  (expected 9 for hwbinder)")
        print(f"    x1(cmd) = 0x{x1:x}  (0xc0306201 = BINDER_WRITE_READ)")
        print(f"    x2(bwr) = 0x{x2:x}  (ptr to binder_write_read struct)")
        print(f"    x8(sc#) = 0x{x8:x}  (29=ioctl confirmed)")
        print(f"    sp      = 0x{sp:x}")
    else:
        print("    ERROR: could not read pt_regs for PID 434")
        regs434 = {}

    # Decode PID 434's BWR
    print("\n[3] Decoding PID 434 binder_write_read...")
    if regs434 and pgd_pa:
        bwr_va = regs434.get('x2', 0)
        if bwr_va:
            decode_binder_write_read(s, f, pgd_pa, bwr_va, "PID434 BWR")
        else:
            print("    ERROR: x2=0 (no BWR ptr in registers)")
    else:
        print("    ERROR: missing pt_regs or PGD")

    # Also read PID 255 pt_regs to confirm it's in binder receive
    print("\n[4] Confirming PID 255 allocator state...")
    alloc_pa = kpa(ALLOC_TASK_VA)
    regs255 = read_pt_regs(s, f, alloc_pa)
    if regs255:
        print(f"    PID 255 PC=0x{regs255['pc']:x}, x8={regs255['x8']} (29=ioctl), fd={regs255['x0']}")
        print(f"    Allocator is in binder receive on fd={regs255['x0']}")

    # Look for PID 434's binder_proc (fd=9, private_data = binder_proc)
    print("\n[5] Finding PID 434 binder_proc (fd=9 private_data)...")
    bproc_va, fd_idx, pid_off = find_binder_proc_wide(s, f, ba_main_pa, 246)
    if not bproc_va:
        # Try PID 434's own task for files
        bproc_va, fd_idx, pid_off = find_binder_proc_wide(s, f, p434_pa, 434)
    if bproc_va:
        print(f"    Found binder_proc @ 0x{bproc_va:x} (fd[{fd_idx}], pid at +0x{pid_off:x})")
        print(f"\n[6] Walking binder_proc->threads for PID 434's binder_thread...")
        inspect_binder_thread_txn(s, f, bproc_va, 434, pid_off)
    else:
        print("    Could not find binder_proc via fd scan.")
        print("    Direct inspection: scanning BA main fd array for fd[9]...")
        # Direct fd[9] inspection
        fd_arr_pa = None
        for off in range(0x4f0, 0x5a0, 8):
            v = r64(s, f, ba_main_pa + off)
            if not is_kva(v): continue
            fdt = r64(s, f, kpa(v) + 0x20)
            if not is_kva(fdt): continue
            mf = r32(s, f, kpa(fdt))
            if 4 <= mf <= 65536:
                farr = r64(s, f, kpa(fdt) + 8)
                if is_kva(farr):
                    fd_arr_pa = kpa(farr)
                    print(f"    fd_array found at PA 0x{fd_arr_pa:x}, max_fds={mf}")
                    break

        if fd_arr_pa:
            # Read fd[9] directly
            fp9 = r64(s, f, fd_arr_pa + 9 * 8)
            print(f"    fd[9] struct file ptr = 0x{fp9:x}")
            if is_kva(fp9):
                fp9_pa = kpa(fp9)
                print(f"    Dumping struct file at fd[9] (first 256 bytes):")
                for off in range(0, 256, 8):
                    v = r64(s, f, fp9_pa + off)
                    if v != 0:
                        kva_flag = " <- KVA" if is_kva(v) else ""
                        print(f"      file+0x{off:03x}: 0x{v:016x}{kva_flag}")

    # VERDICT
    print("\n" + "=" * 70)
    print("VERDICT")
    print("=" * 70)
    print()
    print("  TRUE DEADLOCK CHAIN:")
    print("  PID 417,428,429 (ANGLE workers)  -> condvar wait")
    print("        |                                    ^")
    print("        |                        waiting for PID 434 to signal")
    print("        v")
    print("  PID 434 (ANGLE EGL/gralloc thread) -> ioctl(fd=9, BINDER_WRITE_READ)")
    print("        |                                    <- sends transaction to allocator")
    print("        v")
    print("  PID 255 (IAllocator service)      -> ioctl(fd=4, BINDER_WRITE_READ)")
    print("        |                                    <- in binder RECEIVE, never wakes")
    print("        v")
    print("  TRANSACTION IS NEVER DELIVERED (Outcome A still applies, but for PID 434->255)")
    print()
    print("  => See BC_TRANSACTION target.handle above to identify sub-cause:")
    print("     A1: Wrong handle (resolves to dead/wrong node)")
    print("     A2: binder_node->proc is NULL")
    print("     A3: Work queued to wrong proc's todo")
    print("     A4: Allocator thread in BINDER_LOOPER_STATE_INVALID")
    print("=" * 70)

    s.close()

if __name__ == "__main__":
    main()
