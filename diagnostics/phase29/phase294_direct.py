#!/usr/bin/env python3
# phase294_direct.py
# Phase 29.4: Direct, minimal binder IPC boundary probe.
#
# KEY INSIGHT FROM PRIOR SCRIPTS:
#   - BA MAIN task (PID 246) @ 0xffffffc0ee51ec00 -> PROVEN to work for fd scan
#   - BA WORKER task (PID 417) @ 0xffffffc0fb178000 -> PROVEN to work for u_PC read
#   - All threads in a process share the same files_struct via CLONE_FILES
#   - files_struct offset in task_struct: 0x4f0-0x560 (inspect_fd9.py confirmed)
#   - binder_proc.pid field: +0xb0 (inspect_fd9.py confirmed)
#   - pt_regs PC for worker: kstack+0x3000 page, offset 0xec0+0x100 (decode_ba_bwr_live.py)
#
# GOAL: Determine if PID 255's binder_proc->todo is empty (Outcome A).

import vm_utils
import time
import sys

sys.stdout.reconfigure(line_buffering=True)

TTBR1_PA = 0x43006000

# Task VAs (all from Phase 29.3 baseline, confirmed with CP-02)
BA_MAIN_TASK_VA  = 0xffffffc0ee51ec00  # PID 246 (ba main, proven for fd scan)
WORKER_TASK_VA   = 0xffffffc0fb178000  # PID 417 (ba worker, proven for u_PC)
ALLOC_TASK_VA    = 0xffffffc0ee518000  # PID 255 (allocator main)
ALLOC_THREAD_VAS = [
    (302, 0xffffffc0ea81b600),
    (303, 0xffffffc0ea81c380),
    (313, 0xffffffc0ea86b600),
]
IOCTL_PC = 0x7077008970  # __ioctl in libc (hwbinder wait)

def kpa(va):
    return vm_utils.kva_to_phys(va)

def r64(s, f, pa):
    return vm_utils.read_phys_u64(s, f, pa)

def r32(s, f, pa):
    return vm_utils.read_phys_u32(s, f, pa)

def is_kva(v):
    return 0xffffff8000000000 <= v <= 0xfffffffe00000000

def read_pc(s, f, task_pa):
    """Read saved userspace PC from kernel stack top (pt_regs)."""
    stack_va = r64(s, f, task_pa + 0x60)
    if not stack_va:
        return 0
    pa_top = vm_utils.walk_kernel_pt(s, f, TTBR1_PA, stack_va + 0x3000)
    if not pa_top:
        return 0
    return r64(s, f, pa_top + 0xec0 + 0x100)

def find_files_and_fdarray(s, f, task_pa):
    """
    Find files_struct and fd array using the PROVEN scan from inspect_fd9.py.
    Returns (fd_array_pa, max_fds) or (None, 0).
    """
    for off in range(0x4f0, 0x580, 8):
        v = r64(s, f, task_pa + off)
        if not is_kva(v):
            continue
        pa_f = kpa(v)
        # files_struct.fdt pointer at +0x20
        fdt = r64(s, f, pa_f + 0x20)
        if not is_kva(fdt):
            continue
        pa_fdt = kpa(fdt)
        max_fds = r32(s, f, pa_fdt)
        if 4 <= max_fds <= 65536:
            fd_arr_va = r64(s, f, pa_fdt + 8)
            if is_kva(fd_arr_va):
                print(f"    files_struct @ 0x{v:x} (task+0x{off:x}), fdt @ 0x{fdt:x}, "
                      f"max_fds={max_fds}, fd_array @ 0x{fd_arr_va:x}")
                return kpa(fd_arr_va), max_fds
    return None, 0

def scan_fds_for_binder_proc(s, f, fd_arr_pa, max_fds, target_pid):
    """
    Scan up to max_fds file descriptors for a binder_proc whose .pid == target_pid.
    Returns binder_proc_va or 0.
    """
    limit = min(max_fds, 64)
    for fd in range(limit):
        file_ptr = r64(s, f, fd_arr_pa + fd * 8)
        if not is_kva(file_ptr):
            continue
        file_pa = kpa(file_ptr)
        # private_data offset in struct file: try 0xb0, 0xa0, 0xc8, 0xd0
        for pd_off in (0xb0, 0xa0, 0xc8, 0xd0, 0xc0):
            priv = r64(s, f, file_pa + pd_off)
            if not is_kva(priv):
                continue
            bproc_pa = kpa(priv)
            pid_val = r32(s, f, bproc_pa + 0xb0)  # binder_proc.pid at +0xb0
            if pid_val == target_pid:
                print(f"    Found binder_proc @ 0x{priv:x} (fd[{fd}], file+0x{pd_off:x}), pid={pid_val}")
                return priv
    return 0

def scan_proc_todo(s, f, bproc_va, label):
    """
    Walk binder_proc structure for non-empty list_heads.
    A non-empty list_head at offset X has:
      - list_head.next != (bproc_va + X)  AND  is_kva(next)
    Returns (any_non_empty, list of (offset, next_ptr, binder_work_type)).
    """
    bproc_pa = kpa(bproc_va)
    print(f"\n  [{label}] binder_proc @ 0x{bproc_va:x} (PA 0x{bproc_pa:x})")

    non_empty = []
    WORK_TYPES = {0:'NOOP', 1:'TRANSACTION', 2:'TXN_COMPLETE', 3:'ERROR',
                  4:'CMD_COMPLETE', 5:'RETURN_ERROR', 6:'DEAD_BINDER', 7:'CLEAR_DEATH'}

    for off in range(0x40, 0x300, 8):
        nxt = r64(s, f, bproc_pa + off)
        prv = r64(s, f, bproc_pa + off + 8)
        self_ref = bproc_va + off

        if nxt == self_ref and prv == self_ref:
            # Empty list_head - just record for now
            continue

        if is_kva(nxt) and is_kva(prv) and nxt != 0 and prv != 0:
            # Possibly a non-empty list. Try to walk it.
            # binder_work.entry is at +0x08, so binder_work = node - 0x08
            work_va = nxt - 0x08
            if is_kva(work_va):
                work_pa = kpa(work_va)
                wtype = r32(s, f, work_pa)
                if 0 <= wtype <= 10:
                    wname = WORK_TYPES.get(wtype, f'type_{wtype}')
                    non_empty.append((off, nxt, wtype, wname, work_va))
                    print(f"    NON-EMPTY list_head at +0x{off:x}: next=0x{nxt:x}, "
                          f"binder_work @ 0x{work_va:x}: {wname}")
                    if wtype == 1:  # BINDER_WORK_TRANSACTION
                        read_txn(s, f, work_va)

    if not non_empty:
        print(f"    All list_heads in binder_proc are EMPTY (no pending work).")

    return len(non_empty) > 0, non_empty

def read_txn(s, f, work_va):
    """
    work is at offset +0x08 in binder_transaction.
    So binder_transaction base = work_va - 0x08.
    Layout:
      +0x00: debug_id (int)
      +0x08: binder_work (type at +0x08)
      +0x18: from_thread (binder_thread*)
      +0x28: to_proc (binder_proc*)
      +0x30: to_thread (binder_thread*)
      +0x40: code (u32)
      +0x44: flags (u32)
    """
    txn_va = work_va - 0x08
    txn_pa = kpa(txn_va)
    dbg_id   = r32(s, f, txn_pa + 0x00)
    from_t   = r64(s, f, txn_pa + 0x18)
    to_proc  = r64(s, f, txn_pa + 0x28)
    to_thread= r64(s, f, txn_pa + 0x30)
    code     = r32(s, f, txn_pa + 0x40)
    flags    = r32(s, f, txn_pa + 0x44)
    print(f"      binder_transaction @ 0x{txn_va:x}:")
    print(f"        debug_id={dbg_id}")
    print(f"        from_thread=0x{from_t:x}")
    print(f"        to_proc=0x{to_proc:x}")
    print(f"        to_thread=0x{to_thread:x} {'(unassigned)' if to_thread == 0 else ''}")
    print(f"        code=0x{code:x}, flags=0x{flags:x}")

def walk_proc_threads_rb(s, f, bproc_pa, label):
    """Walk proc->threads RB-tree (at +0x10 in binder_proc)."""
    print(f"\n  Walking {label} thread RB-tree...")
    rb_root_pa = bproc_pa + 0x10
    root_node = r64(s, f, rb_root_pa)
    if not is_kva(root_node):
        print(f"    RB-tree empty or invalid root=0x{root_node:x}")
        return []

    stack = [root_node]
    visited = set()
    threads = []

    while stack and len(visited) < 32:
        node_va = stack.pop()
        if node_va in visited or not is_kva(node_va):
            continue
        visited.add(node_va)
        node_pa = kpa(node_va)

        # rb_node: [0]=parent_color, [1]=right, [2]=left
        rb_right = r64(s, f, node_pa + 8)
        rb_left  = r64(s, f, node_pa + 16)

        # binder_thread.rb_node at +0x08, so thread = node - 0x08
        thread_va = node_va - 0x08
        thread_pa = kpa(thread_va)
        thread_pid = r32(s, f, thread_pa + 0x20)  # binder_thread.pid at +0x20
        txn_stack  = r64(s, f, thread_pa + 0x28)  # transaction_stack at +0x28
        todo_va    = thread_va + 0x30              # thread.todo list_head

        print(f"    binder_thread: pid={thread_pid}, va=0x{thread_va:x}")
        print(f"      transaction_stack=0x{txn_stack:x} {'<- LIVE TXN' if is_kva(txn_stack) else '(null)'}")
        if is_kva(txn_stack):
            read_txn(s, f, txn_stack + 0x08)  # work is at +0x08 in transaction

        # Check thread->todo
        todo_next = r64(s, f, kpa(todo_va))
        if todo_next != todo_va and is_kva(todo_next):
            work_va = todo_next - 0x08
            if is_kva(work_va):
                wtype = r32(s, f, kpa(work_va))
                WORK_TYPES = {1:'TRANSACTION', 2:'TXN_COMPLETE', 3:'ERROR'}
                print(f"      thread.todo: NON-EMPTY! type={WORK_TYPES.get(wtype, wtype)}")
                if wtype == 1:
                    read_txn(s, f, work_va)
        else:
            print(f"      thread.todo: EMPTY")

        threads.append((thread_pid, thread_va, thread_pa))
        if is_kva(rb_right):
            stack.append(rb_right)
        if is_kva(rb_left):
            stack.append(rb_left)

    return threads


def main():
    print("=" * 72)
    print("=== PHASE 29.4: DIRECT BINDER IPC BOUNDARY PROBE ===")
    print("=" * 72)

    s, f = vm_utils.connect_qmp()

    # 1. Restore CP-02
    print("\n[1] Restoring CP-02 (cp02_rtr_token)...")
    res = vm_utils.qmp_hmp(s, f, "loadvm cp02_rtr_token")
    print(f"    loadvm: '{res.strip()}'")

    # 2. Run for 30s so BootAnimation definitely reaches eglInitialize stall
    SETTLE = 30
    print(f"\n[2] Resuming VM for {SETTLE}s ...")
    vm_utils.qmp_hmp(s, f, "c")
    time.sleep(SETTLE)
    vm_utils.qmp_hmp(s, f, "stop")
    print(f"    VM stopped after {SETTLE}s.")

    # 3. Check BA worker (PID 417) u_PC
    print("\n[3] Reading PID 417 (worker) saved PC...")
    worker_pa = kpa(WORKER_TASK_VA)
    upc_417 = read_pc(s, f, worker_pa)
    stall_ok = (upc_417 == IOCTL_PC)
    print(f"    PID 417 u_PC=0x{upc_417:x} {'-> AT IOCTL STALL (eglInitialize blocked)' if stall_ok else '-> NOT YET STALLED'}")

    # Check allocator threads
    print("\n[4] Reading allocator thread PCs (PID 255, 302, 303, 313)...")
    alloc_255_pa = kpa(ALLOC_TASK_VA)
    upc_255 = read_pc(s, f, alloc_255_pa)
    print(f"    PID 255 u_PC=0x{upc_255:x} {'-> SLEEPING(__ioctl)' if upc_255 == IOCTL_PC else '-> ACTIVE'}")
    all_alloc_idle = (upc_255 == IOCTL_PC)
    for pid, va in ALLOC_THREAD_VAS:
        upc = read_pc(s, f, kpa(va))
        idle = (upc == IOCTL_PC)
        if not idle:
            all_alloc_idle = False
        print(f"    PID {pid} u_PC=0x{upc:x} {'-> SLEEPING(__ioctl)' if idle else '-> ACTIVE'}")

    # 5. Find binder_proc for BA via BA MAIN task (0xffffffc0ee51ec00)
    print("\n[5] Finding BootAnimation binder_proc via BA main task (PID 246)...")
    ba_main_pa = kpa(BA_MAIN_TASK_VA)
    fd_arr_pa, max_fds = find_files_and_fdarray(s, f, ba_main_pa)
    ba_bproc_va = 0
    if fd_arr_pa:
        # BA PID is 246 in the main thread; binder_proc covers all threads including 417
        # Try to find any binder_proc for the BA process (pid 246 or 417)
        ba_bproc_va = scan_fds_for_binder_proc(s, f, fd_arr_pa, max_fds, 246)
        if not ba_bproc_va:
            ba_bproc_va = scan_fds_for_binder_proc(s, f, fd_arr_pa, max_fds, 417)
        if not ba_bproc_va:
            print("    NOTE: Could not match by pid, scanning any binder fd...")
            # Accept any binder_proc-like structure
            for fd in range(min(max_fds, 32)):
                file_ptr = r64(s, f, fd_arr_pa + fd * 8)
                if not is_kva(file_ptr):
                    continue
                file_pa = kpa(file_ptr)
                for pd_off in (0xb0, 0xa0, 0xc8, 0xd0):
                    priv = r64(s, f, file_pa + pd_off)
                    if not is_kva(priv):
                        continue
                    bproc_pa = kpa(priv)
                    pid_val = r32(s, f, bproc_pa + 0xb0)
                    if 200 <= pid_val <= 500:
                        print(f"    Found binder-like struct @ 0x{priv:x} (fd[{fd}], file+0x{pd_off:x}), pid_field={pid_val}")
                        if pid_val in (246, 417):
                            ba_bproc_va = priv
                            break
                if ba_bproc_va:
                    break
    else:
        print("    WARNING: fd array not found via BA main task.")
        print("    Attempting wider scan (0x400-0x700)...")
        for off in range(0x400, 0x700, 8):
            v = r64(s, f, ba_main_pa + off)
            if not is_kva(v):
                continue
            pa_f = kpa(v)
            fdt = r64(s, f, pa_f + 0x20)
            if not is_kva(fdt):
                continue
            pa_fdt = kpa(fdt)
            max_fds2 = r32(s, f, pa_fdt)
            if 4 <= max_fds2 <= 65536:
                fd_arr_va = r64(s, f, pa_fdt + 8)
                if is_kva(fd_arr_va):
                    print(f"    FOUND files_struct @ 0x{v:x} (task+0x{off:x}), max_fds={max_fds2}")
                    fd_arr_pa = kpa(fd_arr_va)
                    max_fds = max_fds2
                    ba_bproc_va = scan_fds_for_binder_proc(s, f, fd_arr_pa, max_fds, 246)
                    if not ba_bproc_va:
                        ba_bproc_va = scan_fds_for_binder_proc(s, f, fd_arr_pa, max_fds, 417)
                    break

    if ba_bproc_va:
        scan_proc_todo(s, f, ba_bproc_va, "BA binder_proc (PID 246/417)")
        ba_bproc_pa = kpa(ba_bproc_va)
        walk_proc_threads_rb(s, f, ba_bproc_pa, "BA binder_proc")

    # 6. Find binder_proc for allocator (PID 255)
    print("\n[6] Finding PID 255 (allocator) binder_proc...")
    alloc_main_pa = kpa(ALLOC_TASK_VA)
    # Try files_struct from allocator main task
    alloc_fd_arr_pa, alloc_max_fds = find_files_and_fdarray(s, f, alloc_main_pa)
    alloc_bproc_va = 0
    if alloc_fd_arr_pa:
        alloc_bproc_va = scan_fds_for_binder_proc(s, f, alloc_fd_arr_pa, alloc_max_fds, 255)
    if not alloc_bproc_va:
        print("    Trying wider scan on allocator task (0x400-0x700)...")
        for off in range(0x400, 0x700, 8):
            v = r64(s, f, alloc_main_pa + off)
            if not is_kva(v):
                continue
            pa_f = kpa(v)
            fdt = r64(s, f, pa_f + 0x20)
            if not is_kva(fdt):
                continue
            pa_fdt = kpa(fdt)
            max_fds2 = r32(s, f, pa_fdt)
            if 4 <= max_fds2 <= 65536:
                fd_arr_va = r64(s, f, pa_fdt + 8)
                if is_kva(fd_arr_va):
                    print(f"    FOUND files_struct @ 0x{v:x} (task+0x{off:x}), max_fds={max_fds2}")
                    alloc_fd_arr_pa = kpa(fd_arr_va)
                    alloc_max_fds = max_fds2
                    alloc_bproc_va = scan_fds_for_binder_proc(s, f, alloc_fd_arr_pa, alloc_max_fds, 255)
                    break

    alloc_proc_has_work = False
    if alloc_bproc_va:
        alloc_proc_has_work, _ = scan_proc_todo(s, f, alloc_bproc_va, "PID 255 binder_proc")
        alloc_bproc_pa = kpa(alloc_bproc_va)
        walk_proc_threads_rb(s, f, alloc_bproc_pa, "PID 255 binder_proc")
    else:
        print("    Could not locate PID 255 binder_proc via fd scan.")
        print("    Falling back to u_PC-based verdict (all allocator threads at __ioctl).")

    # 7. VERDICT
    print("\n" + "=" * 72)
    print("=== PHASE 29.4 VERDICT ===")
    print("=" * 72)

    print(f"\n  BA Worker (PID 417)   u_PC = 0x{upc_417:x}  -> {'STALLED in __ioctl' if stall_ok else 'NOT YET AT IOCTL'}")
    print(f"  Allocator (PID 255)  u_PC = 0x{upc_255:x}  -> {'SLEEPING(__ioctl)' if upc_255 == IOCTL_PC else 'ACTIVE'}")
    for pid, va in ALLOC_THREAD_VAS:
        upc = read_pc(s, f, kpa(va))
        print(f"  Allocator (PID {pid})   u_PC = 0x{upc:x}  -> {'SLEEPING(__ioctl)' if upc == IOCTL_PC else 'ACTIVE'}")

    print()
    if all_alloc_idle and not alloc_proc_has_work:
        print("  OUTCOME A -- CONFIRMED")
        print("  All allocator threads sleeping in __ioctl.")
        print("  binder_proc->todo is EMPTY.")
        print("  Binder NEVER delivered the transaction to PID 255.")
        print()
        print("  Next: find WHY. Check the handle in PID 417's BC_TRANSACTION.")
    elif alloc_proc_has_work:
        print("  OUTCOME B/C -- Allocator received transaction (proc->todo non-empty).")
        print("  Check binder_transaction details above.")
    elif not all_alloc_idle:
        print("  OUTCOME B/C -- An allocator thread is ACTIVE (non-ioctl PC).")
    else:
        print("  INCONCLUSIVE (could not locate binder_proc for PID 255).")
        if all_alloc_idle:
            print("  STRONGLY SUGGESTS OUTCOME A (all allocator threads idle).")

    print("=" * 72)
    s.close()


if __name__ == "__main__":
    main()
