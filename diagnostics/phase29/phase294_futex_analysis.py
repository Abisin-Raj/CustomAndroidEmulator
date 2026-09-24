#!/usr/bin/env python3
# phase294_futex_analysis.py  -- Phase 29.4 pivot: futex deadlock analysis
#
# KEY FINDING: PID 417 (BA worker) is in futex(WAIT_BITSET|PRIVATE) NOT ioctl.
#   - PID 417: x8=98 (futex), x0=0xb400007362f75b78 (futex addr), x1=0x89
#   - PID 255: x8=29 (ioctl), fd=4 (binder receive, idle)
#   => Stall is a futex deadlock in ANGLE code, NOT Binder IPC.
#
# THIS SCRIPT:
#   1. Finds ALL threads in BA process (PID 246 thread_group walk).
#   2. For each thread: reads PC, syscall#, futex addr if applicable.
#   3. Reads the futex word at 0x7362f75b78 to find the mutex holder TID.
#   4. Identifies which thread should be waking PID 417.
#   5. VMA-identifies the PC addresses for all BA threads.

import vm_utils
import time
import sys
sys.stdout.reconfigure(line_buffering=True)

TTBR1_PA        = 0x43006000
BA_MAIN_TASK_VA = 0xffffffc0ee51ec00   # PID 246 (BA main, thread group head)
WORKER_TASK_VA  = 0xffffffc0fb178000   # PID 417 (BA worker, confirmed futex)

# Futex address from PID 417's x0 (strip MTE tag if present: top byte = tag)
FUTEX_VA_RAW    = 0xb400007362f75b78
FUTEX_VA        = FUTEX_VA_RAW & 0x0000ffffffffffff   # mask off MTE tag

def kpa(va):  return vm_utils.kva_to_phys(va)
def r64(s,f,pa): return vm_utils.read_phys_u64(s,f,pa)
def r32(s,f,pa): return vm_utils.read_phys_u32(s,f,pa)
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
    """Read pt_regs from kernel stack top. Returns dict or {}."""
    sv = r64(s, f, task_pa + 0x60)
    if not sv: return {}
    pa = walk_kpt(s, f, sv + 0x3000)
    if not pa: return {}
    base = pa + 0xec0
    regs = {}
    for i in range(31):
        regs[f'x{i}'] = r64(s, f, base + i * 8)
    regs['sp']      = r64(s, f, base + 0xF8)
    regs['pc']      = r64(s, f, base + 0x100)
    regs['pstate']  = r64(s, f, base + 0x108)
    return regs

def get_pgd(s, f, task_pa):
    mm_va = r64(s, f, task_pa + 0x470)
    if not is_kva(mm_va): return None
    mm_pa = kpa(mm_va)
    pgd_kva = r64(s, f, mm_pa + 0x48)
    if not is_kva(pgd_kva): return None
    return kpa(pgd_kva)

def get_comm(s, f, task_pa):
    """Read task_struct.comm (16 bytes at +0x6b8)."""
    res = vm_utils.qmp_hmp(s, f, f"xp /2gx 0x{task_pa + 0x6b8:x}")
    chars = []
    for v_str in res.replace(':', ' ').split():
        try:
            v = int(v_str, 16)
            if v == 0 or v > 0xffffffffffffffff:
                continue
            for i in range(8):
                b = (v >> (i*8)) & 0xff
                if b == 0:
                    return ''.join(chars)
                if 32 <= b < 127:
                    chars.append(chr(b))
                else:
                    chars.append('?')
        except ValueError:
            pass
    return ''.join(chars[:16])

def resolve_pc_in_process(s, f, pgd_pa, task_pa, pc):
    """
    Walk the VMA list of a process to find what library/region contains pc.
    Uses mm from the given task_pa.
    """
    if not pgd_pa or pc == 0:
        return f"0x{pc:x}"
    mm_va = r64(s, f, task_pa + 0x470)
    if not is_kva(mm_va): return f"0x{pc:x} (no mm)"
    mm_pa = kpa(mm_va)
    cur_vma = r64(s, f, mm_pa)
    checked = 0
    while is_kva(cur_vma) and checked < 600:
        vma_pa = kpa(cur_vma)
        start = r64(s, f, vma_pa + 0)
        end   = r64(s, f, vma_pa + 8)
        if start <= pc < end:
            vm_file = r64(s, f, vma_pa + 0xa0)
            fname = 'anon'
            if is_kva(vm_file):
                dentry = r64(s, f, kpa(vm_file) + 0x18)
                if is_kva(dentry):
                    np = r64(s, f, kpa(dentry) + 0x28 + 8)
                    if is_kva(np):
                        res = vm_utils.qmp_hmp(s, f, f"xp /16c 0x{kpa(np):x}")
                        chs = []
                        for l in res.splitlines():
                            if ':' not in l: continue
                            for c in l.split(':',1)[1].strip().split():
                                c = c.strip("'")
                                if c in (r'\x00', r'\0', '\\000'): break
                                if len(c) == 1 and 32 <= ord(c) < 127:
                                    chs.append(c)
                                elif c.startswith(r'\x'):
                                    try:
                                        ch = chr(int(c[2:], 16))
                                        if 32 <= ord(ch) < 127:
                                            chs.append(ch)
                                    except: pass
                        fname = ''.join(chs[:32])
            return f"0x{pc:x} ({fname}+0x{pc-start:x})"
        cur_vma = r64(s, f, vma_pa + 16)
        checked += 1
    return f"0x{pc:x} (not mapped)"

def walk_thread_group(s, f, leader_task_va, max_threads=32):
    """
    Walk task_struct->thread_group (list_head at offset 0x5c8).
    Returns list of task_vas in the same process (including leader).
    """
    threads = [leader_task_va]
    seen = {leader_task_va}
    # thread_group list_head at +0x5c8
    TGRP_OFF = 0x5c8
    leader_pa = kpa(leader_task_va)
    cur_node = r64(s, f, leader_pa + TGRP_OFF)   # head.next
    head_node = leader_task_va + TGRP_OFF

    iterations = 0
    while cur_node and cur_node != head_node and iterations < max_threads:
        iterations += 1
        task_va = cur_node - TGRP_OFF
        if task_va in seen:
            break
        if not is_kva(task_va):
            break
        threads.append(task_va)
        seen.add(task_va)
        next_node = r64(s, f, kpa(cur_node))
        cur_node = next_node

    return threads

SYSCALL_NAMES = {
    0: 'io_setup', 1: 'io_destroy', 2: 'io_submit',
    3: 'io_cancel', 4: 'io_getevents', 5: 'setxattr',
    19: 'epoll_create1', 20: 'fcntl', 22: 'open',
    25: 'splice', 28: 'sendfile',
    29: 'ioctl', 30: 'ioprio_set', 31: 'ioprio_get',
    35: 'unlinkat', 49: 'chdir', 50: 'fchdir',
    56: 'read', 57: 'write', 61: 'wait4',
    62: 'kill', 72: 'fcntl', 73: 'flock',
    93: 'exit', 94: 'exit_group',
    96: 'pselect6', 97: 'ppoll', 98: 'futex',
    99: 'nanosleep', 100: 'getitimer',
    113: 'clock_gettime', 114: 'clock_settime',
    115: 'clock_adjtime', 116: 'clock_getres',
    117: 'clock_nanosleep', 118: 'timer_gettime',
    119: 'timer_getoverrun', 120: 'timer_settime',
    121: 'timer_delete', 122: 'clock_adjtime',
    128: 'timerfd_create', 129: 'timerfd_settime',
    130: 'timerfd_gettime', 131: 'signalfd4',
    134: 'signalfd', 160: 'uname',
    172: 'getpid', 178: 'gettid',
    200: 'bind', 202: 'accept', 203: 'connect',
    206: 'sendto', 207: 'recvfrom',
    213: 'setsockopt', 214: 'getsockopt',
    221: 'epoll_pwait', 232: 'epoll_ctl',
    233: 'epoll_wait', 221: 'epoll_pwait',
    260: 'wait4', 261: 'prlimit64',
}
FUTEX_OPS = {
    0: 'WAIT', 1: 'WAKE', 2: 'FD', 3: 'REQUEUE',
    4: 'CMP_REQUEUE', 5: 'WAKE_OP', 6: 'LOCK_PI',
    7: 'UNLOCK_PI', 8: 'TRYLOCK_PI', 9: 'WAIT_BITSET',
    10: 'WAKE_BITSET', 11: 'WAIT_REQUEUE_PI',
    128: 'WAIT_PRIVATE', 129: 'WAKE_PRIVATE',
    137: 'WAIT_BITSET_PRIVATE', 138: 'WAKE_BITSET_PRIVATE',
}


def main():
    s, f = vm_utils.connect_qmp()

    print("=" * 70)
    print("PHASE 29.4 PIVOT: FUTEX DEADLOCK ANALYSIS")
    print("PID 417 is in futex(WAIT_BITSET|PRIVATE), NOT binder ioctl.")
    print("=" * 70)

    # Restore and run to stall
    print("\n[1] Restoring CP-02, running 20s to stall point...")
    vm_utils.qmp_hmp(s, f, "loadvm cp02_rtr_token")
    vm_utils.qmp_hmp(s, f, "c")
    time.sleep(20)
    vm_utils.qmp_hmp(s, f, "stop")
    print("    VM stopped.")

    ba_main_pa = kpa(BA_MAIN_TASK_VA)
    pgd_pa = get_pgd(s, f, ba_main_pa)
    print(f"    BA process PGD PA = 0x{pgd_pa:x}" if pgd_pa else "    ERROR: no PGD")

    # Walk all threads in BA process
    print("\n[2] Walking BA process thread group (from PID 246 leader)...")
    thread_vas = walk_thread_group(s, f, BA_MAIN_TASK_VA)
    print(f"    Found {len(thread_vas)} thread(s) in BA process.")

    # Also make sure we include PID 417 worker
    if WORKER_TASK_VA not in thread_vas:
        thread_vas.append(WORKER_TASK_VA)
        print(f"    Added known worker VA 0x{WORKER_TASK_VA:x}")

    # For each thread: read PC, syscall, futex info
    print("\n[3] Thread state analysis:")
    print(f"  {'task_va':<20s} {'comm':<16s} {'PID':>5s} {'PC':<20s} {'syscall':<20s} notes")
    print("  " + "-" * 100)

    for t_va in thread_vas:
        t_pa = kpa(t_va)
        pid  = r32(s, f, t_pa + 0x518)
        tgid = r32(s, f, t_pa + 0x51c)
        comm = get_comm(s, f, t_pa)
        regs = read_pt_regs(s, f, t_pa)
        if not regs:
            print(f"  0x{t_va:x}  {comm:<16s} {pid:>5d}  (no pt_regs)")
            continue

        pc  = regs['pc']
        x8  = regs['x8']   # syscall number
        x0  = regs['x0']
        x1  = regs['x1']
        x2  = regs['x2']

        sc_name = SYSCALL_NAMES.get(x8, f'sc#{x8}')
        notes = ''

        if x8 == 98:  # futex
            futex_addr = x0 & 0x0000ffffffffffff  # strip MTE tag
            futex_op = FUTEX_OPS.get(x1, f'op=0x{x1:x}')
            notes = f"futex({futex_op}) addr=0x{futex_addr:x}"
            if futex_addr == FUTEX_VA:
                notes += " *** SAME FUTEX AS PID 417 ***"
        elif x8 == 29:  # ioctl
            notes = f"ioctl fd={x0}, cmd=0x{x1:x}"
        elif x8 == 0:
            notes = f"x0=0x{x0:x}"

        # Resolve PC
        pc_sym = resolve_pc_in_process(s, f, pgd_pa, ba_main_pa, pc)

        print(f"  0x{t_va:x}  {comm:<16s} {pid:>5d}  {pc_sym:<48s}  {sc_name:<12s}  {notes}")

    # Read the futex word at FUTEX_VA
    print(f"\n[4] Reading futex word at 0x{FUTEX_VA:x} (PID 417's wait address)...")
    if pgd_pa:
        futex_pa = walk_user_pt(s, f, pgd_pa, FUTEX_VA)
        if futex_pa:
            # Read 32-bit futex word (pthread_mutex state)
            futex_word = r32(s, f, futex_pa)
            print(f"    futex PA = 0x{futex_pa:x}")
            print(f"    futex word (32-bit) = 0x{futex_word:08x} = {futex_word}")
            if futex_word == 0:
                print("    => MUTEX IS UNLOCKED (futex=0). Weird: why is PID 417 waiting?")
            else:
                holder_tid = futex_word & 0x3fffffff
                has_waiters = bool(futex_word & 0x80000000)
                print(f"    => Lock holder TID = {holder_tid}, has_waiters={has_waiters}")
                print(f"       Look for thread with TID={holder_tid} above ^")

            # Also read 8 bytes for condvar analysis
            futex_word64 = r64(s, f, futex_pa)
            print(f"    futex area (64-bit) = 0x{futex_word64:016x}")

            # Read surrounding context (16 bytes before and after)
            if futex_pa >= 16:
                print(f"    Surrounding memory at 0x{FUTEX_VA-16:x}:")
                for off in range(-16, 32, 8):
                    pa_off = futex_pa + off
                    v = r64(s, f, pa_off)
                    marker = " <<< FUTEX" if off == 0 else ""
                    print(f"      +{off:+4d} (0x{FUTEX_VA+off:x}): 0x{v:016x}{marker}")
        else:
            print(f"    ERROR: could not walk user PT for 0x{FUTEX_VA:x}")
    else:
        print("    ERROR: no PGD")

    # Identify the library at PID 417's stall PC
    print(f"\n[5] Identifying PID 417's stall PC=0x75732518a0 and LR=0x7573256184...")
    for addr, label in [(0x75732518a0, "PC"), (0x7573256184, "LR"), (0x75732afd78, "PC@t=50s")]:
        sym = resolve_pc_in_process(s, f, pgd_pa, ba_main_pa, addr)
        print(f"    PID 417 {label}: {sym}")

    # Summary
    print("\n" + "=" * 70)
    print("PHASE 29.4 PIVOT SUMMARY")
    print("=" * 70)
    print()
    print("  CONFIRMED: PID 417 (BA worker) is in futex(WAIT_BITSET|PRIVATE),")
    print("  NOT in binder ioctl. The Phase 29.3 LR-vs-PC confusion is resolved.")
    print()
    print(f"  Futex VA: 0x{FUTEX_VA:x}")
    print("  Futex op: FUTEX_WAIT_BITSET | FUTEX_PRIVATE (0x89)")
    print()
    print("  UPDATED STALL PICTURE:")
    print("  - PID 417 blocked on pthread_cond/mutex in ANGLE (libGLES/Pastel)")
    print("  - PID 255 idle in binder receive (IAllocator never gets a request?)")
    print("  - Root cause: identify who should wake the futex at PID 417's addr")
    print()
    print("  => Next: check the futex word and all BA thread states above.")
    print("     If the mutex holder is NOT in the BA thread group, look for")
    print("     a DIFFERENT process (GPU daemon, hwservicemanager) that should")
    print("     signal this futex but is itself stuck.")
    print("=" * 70)

    s.close()

if __name__ == "__main__":
    main()
