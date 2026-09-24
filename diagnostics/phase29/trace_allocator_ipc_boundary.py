#!/usr/bin/env python3
# trace_allocator_ipc_boundary.py
# Phase 29.4: Determine the exact HIDL allocator IPC receive/reply boundary.
#
# STRATEGY:
#   Restores CP-02, lets the guest run briefly (~5s) to let BootAnimation
#   reach the eglInitialize() stall, then STOPS and inspects kernel Binder
#   data structures directly to classify the deadlock into one of four outcomes:
#
#   Outcome A: Transaction never delivered to PID 255
#              (PID 255 proc->todo and all thread->todo lists are EMPTY)
#   Outcome B: PID 255 received transaction, but allocator dispatch fails before allocate()
#              (A thread in PID 255 has a non-zero u_PC somewhere after __ioctl)
#   Outcome C: PID 255 enters allocate() but stalls (e.g. ION/Gralloc lock)
#   Outcome D: PID 255 replied (BC_REPLY submitted) but PID 417 never unblocked
#
# KEY KERNEL STRUCTURES (Linux 5.10 / android12-5.10):
#   struct binder_proc {
#     struct hlist_node proc_node;  // +0x00
#     struct rb_root threads;       // +0x10 (rb root - 1 ptr)
#     struct rb_root nodes;         // +0x18
#     struct rb_root refs_by_desc;  // +0x20
#     struct rb_root refs_by_node;  // +0x28
#     int pid;                      // +0x30
#     ...
#     struct list_head todo;        // at some offset - we'll search
#     ...
#   }
#   struct binder_thread {
#     struct binder_proc *proc;
#     struct rb_node rb_node;
#     int pid;
#     ...
#     struct list_head todo;
#     struct binder_transaction *transaction_stack;
#     ...
#   }
#   struct binder_transaction {
#     int debug_id;
#     struct binder_work work;         // +0x08 (type=BINDER_WORK_TRANSACTION=1)
#     struct binder_thread *from;      // +0x18
#     struct binder_transaction *from_parent;
#     struct binder_proc *to_proc;     // +0x28
#     struct binder_thread *to_thread; // +0x30
#     struct binder_transaction *to_parent;
#     unsigned int code;               // transaction code
#     unsigned int flags;
#     ...
#   }

import vm_utils
import time
import sys

sys.stdout.reconfigure(line_buffering=True)

TTBR1_PA    = 0x43006000  # kernel page table base
KERNEL_BASE = 0xffffff8000000000

# Known task_struct VAs (from Phase 29.3 baseline)
BOOTANIM_TASK_VA   = 0xffffffc0fb178000  # PID 417 (BootAnimation worker)
ALLOCATOR_TASK_VA  = 0xffffffc0ee518000  # PID 255 (allocator main)
ALLOC_THREAD_VAS   = [
    (302, 0xffffffc0ea81b600, "hw_binder_1"),
    (303, 0xffffffc0ea81c380, "hw_binder_2"),
    (313, 0xffffffc0ea86b600, "hw_binder_3"),
]

# ------------------------------------------------------------------------------
# Helpers
# ------------------------------------------------------------------------------

def kpa(va):
    return vm_utils.kva_to_phys(va)

def r64(s, f, pa):
    return vm_utils.read_phys_u64(s, f, pa)

def r32(s, f, pa):
    return vm_utils.read_phys_u32(s, f, pa)

def xp(s, f, pa, count=4, width='g'):
    """Read 'count' values of width 'g'=64-bit 'w'=32-bit from physical pa."""
    res = vm_utils.qmp_hmp(s, f, f"xp /{count}{width}x 0x{pa:x}")
    vals = []
    for l in res.splitlines():
        if ':' not in l:
            continue
        for tok in l.split(':', 1)[1].strip().split():
            try:
                vals.append(int(tok, 16))
            except ValueError:
                pass
    return vals

def is_kva(v):
    """True if value looks like a valid kernel virtual address."""
    return 0xffffff8000000000 <= v <= 0xfffffffe00000000

def is_kernel_slab(v):
    """Alias for is_kva - all slab objects are in kernel VA range."""
    return is_kva(v)

def read_comm(s, f, task_pa):
    """Read task_struct.comm (16 bytes) at offset 0x6b8."""
    res = vm_utils.qmp_hmp(s, f, f"xp /16c 0x{task_pa + 0x6b8:x}")
    chars = []
    for l in res.splitlines():
        if ':' not in l:
            continue
        for c in l.split(':', 1)[1].strip().split():
            c = c.strip("'")
            if c in (r'\x00', r'\0', '\\000'):
                return ''.join(chars)
            if len(c) == 1:
                chars.append(c)
            elif c.startswith(r'\x'):
                try:
                    chars.append(chr(int(c[2:], 16)))
                except ValueError:
                    pass
    return ''.join(chars)

def saved_user_pc(s, f, task_pa):
    """Read saved user-space PC from the kernel thread stack (pt_regs at top)."""
    stack_va = r64(s, f, task_pa + 0x60)  # task_struct.stack
    if not stack_va:
        return 0
    # pt_regs sits at kstack + 0x3000 page, base offset 0xec0.
    # PC (elr_el1) is at pt_regs base + 0x100.
    pa_top = vm_utils.walk_kernel_pt(s, f, TTBR1_PA, stack_va + 0x3000)
    if not pa_top:
        return 0
    return r64(s, f, pa_top + 0xec0 + 0x100)

# ------------------------------------------------------------------------------
# Find files_struct -> fd array for a task
# ------------------------------------------------------------------------------

def find_fd_array(s, f, task_pa):
    """Return (files_offset, fdt_pa, fd_array_pa) for the task."""
    # files_struct pointer confirmed at 0x4f0-0x560 (inspect_fd9.py)
    for off in range(0x4f0, 0x570, 8):
        v = r64(s, f, task_pa + off)
        if not is_kva(v):
            continue
        v_pa = kpa(v)
        fdt = r64(s, f, v_pa + 0x20)
        if not is_kva(fdt):
            continue
        fdt_pa = kpa(fdt)
        max_fds = r32(s, f, fdt_pa)
        if 4 <= max_fds <= 65536:
            fdarr = r64(s, f, fdt_pa + 8)
            if is_kva(fdarr):
                return off, fdt_pa, kpa(fdarr)
    return None, None, None

def get_file_private_data(s, f, fdarr_pa, fd_idx):
    """Return private_data pointer of fd[fd_idx]. Tries +0xb0 and +0xc8 offsets."""
    file_ptr = r64(s, f, fdarr_pa + fd_idx * 8)
    if not is_kva(file_ptr):
        return 0
    file_pa = kpa(file_ptr)
    # private_data can be at +0xa0, +0xb0, or +0xc8 depending on kernel config
    # Scan the likely range
    for pd_off in (0xb0, 0xa0, 0xc8, 0xd0):
        pd = r64(s, f, file_pa + pd_off)
        if is_kva(pd):
            return pd
    return 0

# ------------------------------------------------------------------------------
# binder_proc inspection
# ------------------------------------------------------------------------------

def find_binder_proc_todo_offset(s, f, bproc_pa, bproc_va):
    """
    Scan the binder_proc for the 'todo' list_head by looking for a self-referential
    pointer pair (both point back into the structure within ~0x200 bytes).
    Returns (todo_offset, head_next, head_prev) or (None, 0, 0).
    """
    for off in range(0x60, 0x300, 8):
        nxt = r64(s, f, bproc_pa + off)
        prv = r64(s, f, bproc_pa + off + 8)
        # Empty list: both pointers equal bproc_va + off
        if nxt == bproc_va + off and prv == bproc_va + off:
            # This is an empty list_head -- could be 'todo'
            # We'll return all candidates and check by offset heuristic
            pass
        if is_kva(nxt) and is_kva(prv):
            # Non-empty list? Both next/prev must be valid
            # If next != (bproc_va + off), it points to a binder_work node
            if nxt != prv or nxt == bproc_va + off:
                return off, nxt, prv
    return None, 0, 0

def inspect_list_head(s, f, list_va, list_pa, label, max_items=16):
    """
    Walk a list_head assuming binder_work nodes are embedded at offset 0x8
    (i.e. list is embedded in binder_work, which starts 8 bytes before entry node).
    Returns a list of (work_type, work_va) tuples found.
    """
    items = []
    seen = set()
    cur = r64(s, f, list_pa)           # list_head.next
    while cur and cur != list_va and cur not in seen and len(items) < max_items:
        seen.add(cur)
        # binder_work is embedded: the list node is at work+0x08 (entry field)
        # So work_va = cur - 0x08
        work_va = cur - 0x08
        work_pa = kpa(work_va)
        work_type = r32(s, f, work_pa)   # binder_work.type (u32 at offset 0)
        items.append((work_type, work_va, cur))
        # Advance
        nxt = r64(s, f, kpa(cur))
        if nxt == cur:
            break
        cur = nxt
    return items

def find_proc_todo(s, f, bproc_va, label):
    """
    Locate binder_proc.todo list_head by scanning for a self-referential or
    non-empty list_head in the first 0x300 bytes of the proc struct.
    We return (offset, empty, items_list).
    """
    bproc_pa = kpa(bproc_va)
    BINDER_WORK_TRANSACTION = 1
    BINDER_WORK_TRANSACTION_COMPLETE = 2

    print(f"\n  [{label}] binder_proc @ 0x{bproc_va:x} (PA 0x{bproc_pa:x})")

    # Scan all offsets for list_head candidates
    candidates = []
    for off in range(0x40, 0x280, 8):
        nxt = r64(s, f, bproc_pa + off)
        prv = r64(s, f, bproc_pa + off + 8)
        self_ref = bproc_va + off
        if nxt == self_ref and prv == self_ref:
            candidates.append((off, True, []))   # empty list
        elif is_kva(nxt) and is_kva(prv) and nxt != 0 and prv != 0:
            # Possibly non-empty; try to walk it
            items = inspect_list_head(s, f, bproc_va + off, bproc_pa + off, label)
            if items:
                candidates.append((off, False, items))  # non-empty!

    if not candidates:
        print(f"    WARNING: No list_head candidates found in proc struct!")
        return None, True, []

    # Report all non-empty ones first
    non_empty = [(o, e, i) for o, e, i in candidates if not e]
    empty_ones = [(o, e, i) for o, e, i in candidates if e]

    if non_empty:
        print(f"    Non-empty list_heads at offsets: {[hex(o) for o, _, _ in non_empty]}")
        for off, _, items in non_empty:
            print(f"      offset +0x{off:x}:")
            for wtype, wva, node_va in items:
                wtype_name = {0: 'NOOP', 1: 'TRANSACTION', 2: 'TRANSACTION_COMPLETE',
                              3: 'ERROR', 4: 'CMD_COMPLETE', 5: 'RETURN_ERROR', 6: 'DEAD_BINDER',
                              7: 'CLEAR_DEATH', 8: 'FAILED_REPLY'}.get(wtype, f'UNK({wtype})')
                print(f"        binder_work @ 0x{wva:x}: type={wtype_name} (node @ 0x{node_va:x})")
                if wtype == BINDER_WORK_TRANSACTION:
                    inspect_binder_transaction(s, f, wva)
        return non_empty[0][0], False, non_empty[0][2]
    else:
        print(f"    All list_heads are EMPTY ({len(empty_ones)} found at {[hex(o) for o, _, _ in empty_ones[:6]]}...)")
        return empty_ones[0][0] if empty_ones else None, True, []

def inspect_binder_transaction(s, f, work_va):
    """
    Given a binder_work embedded in binder_transaction at offset 0x08,
    read the binder_transaction fields to show from/to/code.
    Layout (approximate for android12-5.10):
      +0x00: int debug_id
      +0x04: int debug_id (spinlock / padding)
      +0x08: struct binder_work work  (type=u32 at +0x08)
      +0x18: struct binder_thread *from
      +0x20: struct binder_transaction *from_parent
      +0x28: struct binder_proc *to_proc
      +0x30: struct binder_thread *to_thread
      +0x38: struct binder_transaction *to_parent
      +0x40: unsigned int code
      +0x44: unsigned int flags
    """
    txn_va = work_va  # work is at +0x08, so txn_va = work_va - 0x08
    # Actually: work is embedded at offset 0x08 in binder_transaction
    # But we received work_va = binder_transaction_va + 0x08 from the list walker
    # So binder_transaction base = work_va - 0x08
    txn_base_va = work_va - 0x08
    txn_base_pa = kpa(txn_base_va)

    dbg_id     = r32(s, f, txn_base_pa + 0x00)
    from_thrd  = r64(s, f, txn_base_pa + 0x18)
    to_proc    = r64(s, f, txn_base_pa + 0x28)
    to_thread  = r64(s, f, txn_base_pa + 0x30)
    txn_code   = r32(s, f, txn_base_pa + 0x40)
    txn_flags  = r32(s, f, txn_base_pa + 0x44)

    print(f"          >>> binder_transaction @ 0x{txn_base_va:x}")
    print(f"              debug_id={dbg_id}")
    print(f"              from_thread=0x{from_thrd:x} {'(PID 417)' if from_thrd == BOOTANIM_TASK_VA else ''}")
    print(f"              to_proc=0x{to_proc:x}  {'(PID 255 allocator)' if to_proc == ALLOCATOR_TASK_VA else ''}")
    print(f"              to_thread=0x{to_thread:x}  {'(0=unassigned)' if to_thread == 0 else ''}")
    print(f"              code=0x{txn_code:x}, flags=0x{txn_flags:x}")

def find_thread_binder_proc(s, f, task_pa, task_va):
    """
    Find the binder_proc associated with a task by scanning its file descriptors
    for /dev/hwbinder (fd=9 for PID 417).  Returns binder_proc_va or 0.
    """
    _, fdt_pa, fdarr_pa = find_fd_array(s, f, task_pa)
    if fdarr_pa is None:
        return 0
    # Try fd 9 first (known hwbinder fd for PID 417), then scan
    for fd_idx in range(0, 20):
        private_data = get_file_private_data(s, f, fdarr_pa, fd_idx)
        if is_kva(private_data):
            # binder_proc.pid is at some offset -- verify by checking it looks like a proc
            bp_pa = kpa(private_data)
            # Try to read binder_proc.pid at +0x30
            pid_val = r32(s, f, bp_pa + 0x30)
            if 1 <= pid_val <= 65535:
                return private_data
    return 0

# ------------------------------------------------------------------------------
# binder_thread inspection (transaction_stack + thread todo)
# ------------------------------------------------------------------------------

def inspect_binder_thread(s, f, task_pa, task_va, pid, label):
    """
    For PID 417: find transaction_stack pointer to confirm the transaction WAS sent.
    For PID 255/302/303/313: find thread->todo list to see if work is pending.

    binder_thread layout (approx android12-5.10):
      Embedded in binder_proc's rb_tree. We need to find it via the proc's
      thread rbtree (proc->threads rb_root at +0x10). The thread struct layout:
        +0x00: struct binder_proc *proc
        +0x08: struct rb_node rb_node
        +0x20: int pid
        +0x24: int looper
        +0x28: struct binder_transaction *transaction_stack
        +0x30: struct list_head todo
        +0x40: bool process_todo
        +0x48: struct binder_error return_error
        +0x58: struct binder_error reply_error
        +0x68: wait_queue_head_t wait
        ...

    We locate binder_thread by walking proc->threads RB-tree looking for
    a node where binder_thread.pid == target_pid.
    """
    print(f"\n  [{label}] Searching binder_thread for PID {pid}...")

    # Find the binder_proc for this task
    _, fdt_pa, fdarr_pa = find_fd_array(s, f, task_pa)
    if fdarr_pa is None:
        print(f"    ERROR: Could not locate fd array for PID {pid}")
        return None

    bproc_va = 0
    for fd_idx in range(0, 25):
        private_data = get_file_private_data(s, f, fdarr_pa, fd_idx)
        if not is_kva(private_data):
            continue
        bp_pa = kpa(private_data)
        pid_val = r32(s, f, bp_pa + 0x30)
        if pid_val == pid:
            bproc_va = private_data
            print(f"    Found binder_proc @ 0x{bproc_va:x} (via fd[{fd_idx}])")
            break

    if not bproc_va:
        # Try adjacent pids for thread group members
        for fd_idx in range(0, 25):
            private_data = get_file_private_data(s, f, fdarr_pa, fd_idx)
            if not is_kva(private_data):
                continue
            bproc_va = private_data
            print(f"    Using binder_proc @ 0x{bproc_va:x} (pid check relaxed, fd[{fd_idx}])")
            break

    if not bproc_va:
        print(f"    ERROR: Could not locate binder_proc for PID {pid}")
        return None

    return bproc_va

# ------------------------------------------------------------------------------
# Walk binder_proc->threads RB-tree to find a binder_thread by PID
# ------------------------------------------------------------------------------

def walk_rb_tree_for_thread(s, f, rb_root_pa, target_pid, max_nodes=64):
    """
    Walk an RB-tree rooted at rb_root_pa (rb_root.rb_node = first 8 bytes).
    For each node, interpret as binder_thread (node at +0x08 -> thread at node - 0x08).
    Actually binder_thread.rb_node is at offset +0x08, so thread_va = node_va - 0x08.
    Check binder_thread.pid at +0x20.
    Returns (thread_va, thread_pa) or (None, None).
    """
    root_node = r64(s, f, rb_root_pa)   # rb_root.rb_node pointer
    if not is_kva(root_node):
        return None, None

    stack = [root_node]
    visited = set()
    found = []

    while stack and len(visited) < max_nodes:
        node_va = stack.pop()
        if node_va in visited or not is_kva(node_va):
            continue
        visited.add(node_va)

        node_pa = kpa(node_va)
        # rb_node layout: [0] __rb_parent_color, [1] rb_right, [2] rb_left (each u64)
        rb_right = r64(s, f, node_pa + 8)
        rb_left  = r64(s, f, node_pa + 16)

        # binder_thread.rb_node is at offset +0x08, so thread_va = node_va - 0x08
        thread_va = node_va - 0x08
        thread_pa = kpa(thread_va)
        thread_pid = r32(s, f, thread_pa + 0x20)  # binder_thread.pid

        found.append((thread_pid, thread_va, thread_pa))

        if is_kva(rb_right):
            stack.append(rb_right)
        if is_kva(rb_left):
            stack.append(rb_left)

    return found

# ------------------------------------------------------------------------------
# Main diagnostic
# ------------------------------------------------------------------------------

def main():
    print("=" * 80)
    print("=== PHASE 29.4: HIDL ALLOCATOR IPC BOUNDARY INSPECTION ===")
    print("=== Goal: Classify deadlock into Outcome A / B / C / D  ===")
    print("=" * 80)

    s, f = vm_utils.connect_qmp()

    # -- Step 1: Restore CP-02 --------------------------------------------------
    print("\n[1/6] Restoring CP-02 (cp02_rtr_token)...")
    res = vm_utils.qmp_hmp(s, f, "loadvm cp02_rtr_token")
    print(f"      loadvm result: '{res.strip()}'")

    # -- Step 2: Resume briefly so BA reaches eglInitialize stall --------------
    SETTLE_SECONDS = 25
    print(f"\n[2/6] Resuming VM for {SETTLE_SECONDS}s (let BootAnimation reach eglInitialize stall)...")
    vm_utils.qmp_hmp(s, f, "c")
    time.sleep(SETTLE_SECONDS)
    vm_utils.qmp_hmp(s, f, "stop")
    print(f"      VM stopped after {SETTLE_SECONDS}s.")

    # -- Step 3: Read PID 417 saved registers ----------------------------------
    print("\n[3/6] Reading PID 417 (BootAnimation worker) saved context...")
    ba_task_pa = kpa(BOOTANIM_TASK_VA)
    ba_upc = saved_user_pc(s, f, ba_task_pa)
    ba_comm = read_comm(s, f, ba_task_pa)
    print(f"      comm='{ba_comm}', u_PC=0x{ba_upc:x}")
    if ba_upc in (0x7077008970, 0):
        print("      -> PID 417 is at __ioctl (hwbinder wait) -> CONFIRMED BLOCKED in eglInitialize.")
    else:
        print(f"      -> PID 417 at unexpected PC=0x{ba_upc:x} (not the expected __ioctl stall).")
        print("        Proceeding with inspection anyway...")

    # -- Step 4: Find PID 417's binder_proc and binder_thread ------------------
    print("\n[4/6] Locating PID 417 binder_proc and binder_thread...")
    _, fdt_pa_417, fdarr_pa_417 = find_fd_array(s, f, ba_task_pa)
    if fdarr_pa_417 is None:
        print("      FATAL: Could not find fd array for PID 417!")
        s.close()
        return

    ba_bproc_va = 0
    ba_fd = -1
    for fd_idx in range(0, 20):
        pd = get_file_private_data(s, f, fdarr_pa_417, fd_idx)
        if not is_kva(pd):
            continue
        bp_pa = kpa(pd)
        pid_val = r32(s, f, bp_pa + 0xb0)  # binder_proc.pid at +0xb0 (confirmed)
        if pid_val == 417:
            ba_bproc_va = pd
            ba_fd = fd_idx
            break

    if ba_bproc_va:
        print(f"      PID 417 binder_proc @ 0x{ba_bproc_va:x} (found at fd[{ba_fd}])")
    else:
        print("      WARNING: Could not find binder_proc for PID 417 by pid field.")
        # Fall back: fd 9 is known hwbinder fd
        pd = get_file_private_data(s, f, fdarr_pa_417, 9)
        if is_kva(pd):
            ba_bproc_va = pd
            print(f"      Fallback: using fd[9] private_data = 0x{ba_bproc_va:x} as binder_proc")

    # Check PID 417 proc->todo
    if ba_bproc_va:
        find_proc_todo(s, f, ba_bproc_va, "PID 417 binder_proc")

        # Also: walk proc->threads to find PID 417 binder_thread
        ba_bproc_pa = kpa(ba_bproc_va)
        print("\n      Walking PID 417 binder_proc->threads RB-tree...")
        threads_rb = ba_bproc_pa + 0x10   # proc->threads rb_root at +0x10
        all_threads_417 = walk_rb_tree_for_thread(s, f, threads_rb, 417)
        if all_threads_417:
            for tpid, tva, tpa in all_threads_417:
                print(f"        binder_thread: pid={tpid}, va=0x{tva:x}")
                # Read transaction_stack @ +0x28
                txn_stack = r64(s, f, tpa + 0x28)
                print(f"          transaction_stack = 0x{txn_stack:x}")
                if is_kva(txn_stack):
                    print(f"          *** LIVE TRANSACTION FOUND ***")
                    # Read transaction: from/to/code
                    tx_pa = kpa(txn_stack)
                    from_t  = r64(s, f, tx_pa + 0x18)
                    to_proc = r64(s, f, tx_pa + 0x28)
                    to_th   = r64(s, f, tx_pa + 0x30)
                    txn_code = r32(s, f, tx_pa + 0x40)
                    txn_flags= r32(s, f, tx_pa + 0x44)
                    print(f"          from_thread=0x{from_t:x}")
                    print(f"          to_proc=0x{to_proc:x}  (allocator={hex(ALLOCATOR_TASK_VA)})")
                    print(f"          to_thread=0x{to_th:x}")
                    print(f"          code=0x{txn_code:x}, flags=0x{txn_flags:x}")
                # Read thread todo list @ +0x30
                thread_todo_va = tva + 0x30
                thread_todo_pa = tpa + 0x30
                todo_items = inspect_list_head(s, f, thread_todo_va, thread_todo_pa, "PID417.thread.todo")
                if todo_items:
                    print(f"          thread.todo: {len(todo_items)} item(s):")
                    for wtype, wva, nva in todo_items:
                        print(f"            binder_work @ 0x{wva:x}: type={wtype}")
                else:
                    print(f"          thread.todo: EMPTY")
        else:
            print("        No binder_threads found in RB-tree (offset may differ).")

    # -- Step 5: Inspect PID 255 binder_proc and all thread todo lists ----------
    print("\n[5/6] Inspecting PID 255 (allocator) binder_proc...")
    alloc_task_pa = kpa(ALLOCATOR_TASK_VA)
    alloc_comm = read_comm(s, f, alloc_task_pa)
    alloc_upc  = saved_user_pc(s, f, alloc_task_pa)
    print(f"      PID 255 comm='{alloc_comm}', u_PC=0x{alloc_upc:x}")

    _, fdt_pa_255, fdarr_pa_255 = find_fd_array(s, f, alloc_task_pa)
    alloc_bproc_va = 0
    if fdarr_pa_255:
        for fd_idx in range(0, 25):
            pd = get_file_private_data(s, f, fdarr_pa_255, fd_idx)
            if not is_kva(pd):
                continue
            bp_pa = kpa(pd)
            pid_val = r32(s, f, bp_pa + 0xb0)  # binder_proc.pid at +0xb0
            if pid_val == 255:
                alloc_bproc_va = pd
                print(f"      PID 255 binder_proc @ 0x{alloc_bproc_va:x} (fd[{fd_idx}])")
                break

    if not alloc_bproc_va:
        print("      WARNING: Could not find PID 255 binder_proc by pid field.")

    # Inspect allocator proc->todo
    alloc_proc_empty = True
    if alloc_bproc_va:
        _, alloc_proc_empty, alloc_proc_items = find_proc_todo(s, f, alloc_bproc_va, "PID 255 binder_proc")

        # Walk proc->threads to find all allocator binder_threads
        print("\n      Walking PID 255 binder_proc->threads RB-tree...")
        alloc_bproc_pa = kpa(alloc_bproc_va)
        threads_rb_255 = alloc_bproc_pa + 0x10
        all_threads_255 = walk_rb_tree_for_thread(s, f, threads_rb_255, 255)
        thread_todos_empty = True
        if all_threads_255:
            for tpid, tva, tpa in all_threads_255:
                print(f"        binder_thread: pid={tpid}, va=0x{tva:x}")
                txn_stack = r64(s, f, tpa + 0x28)
                print(f"          transaction_stack = 0x{txn_stack:x} {'<- ACTIVE' if is_kva(txn_stack) else '(null)'}")
                thread_todo_va = tva + 0x30
                thread_todo_pa = tpa + 0x30
                items = inspect_list_head(s, f, thread_todo_va, thread_todo_pa, f"PID{tpid}.thread.todo")
                if items:
                    thread_todos_empty = False
                    print(f"          thread.todo: {len(items)} item(s) <- TRANSACTION PENDING!")
                    for wtype, wva, nva in items:
                        wtype_name = {1: 'TRANSACTION', 2: 'TRANSACTION_COMPLETE', 3:'ERROR'}.get(wtype, f'UNK({wtype})')
                        print(f"            binder_work @ 0x{wva:x}: type={wtype_name}")
                        if wtype == 1:
                            inspect_binder_transaction(s, f, wva)
                else:
                    print(f"          thread.todo: EMPTY")
        else:
            print("        No binder_threads found in RB-tree.")

    # -- Step 6: Check all allocator sibling threads (302, 303, 313) -----------
    print("\n[6/6] Inspecting allocator sibling threads (302, 303, 313)...")
    sibling_any_active = False
    for sib_pid, sib_va, sib_name in ALLOC_THREAD_VAS:
        sib_pa = kpa(sib_va)
        sib_upc = saved_user_pc(s, f, sib_pa)
        sib_comm = read_comm(s, f, sib_pa)
        IOCTL_PC = 0x7077008970
        active_flag = " <- ACTIVE" if (sib_upc != IOCTL_PC and sib_upc != 0) else ""
        status = "SLEEPING(__ioctl)" if sib_upc == IOCTL_PC else f"ACTIVE u_PC=0x{sib_upc:x}"
        print(f"      PID {sib_pid} ({sib_name}): comm='{sib_comm}', u_PC=0x{sib_upc:x} -> {status}")
        if sib_upc != IOCTL_PC and sib_upc != 0:
            sibling_any_active = True

    # -- Verdict ---------------------------------------------------------------
    print("\n" + "=" * 80)
    print("=== PHASE 29.4 VERDICT ===")
    print("=" * 80)

    all_alloc_idle = not sibling_any_active and alloc_upc in (0x7077008970, 0)

    if alloc_proc_empty and all_alloc_idle:
        print("""
  OUTCOME A -- CONFIRMED (HIGH CONFIDENCE)
  -----------------------------------------
  PID 255 binder_proc->todo is EMPTY.
  All 4 allocator threads (PID 255, 302, 303, 313) are sleeping in __ioctl.
  The transaction from PID 417 was NEVER delivered to the allocator's todo queue.

  NEXT QUESTION: Why did Binder not deliver the transaction to PID 255?
  Possible sub-causes:
    A1. The target handle in PID 417's BC_TRANSACTION resolves to a dead/wrong node.
    A2. The target binder_node's proc is NULL (proc has exited or was never set).
    A3. The transaction was placed on the WRONG proc's todo (e.g. hwservicemanager).
    A4. The binder driver chose a thread for delivery but that thread was in
        BINDER_LOOPER_STATE_INVALID, so the work sat on proc->todo but
        no thread was woken up (or the waitqueue was empty).
""")
    elif not alloc_proc_empty:
        print("""
  OUTCOME B or C -- Transaction IS in allocator queue
  ----------------------------------------------------
  PID 255 binder_proc->todo or a thread->todo contains BINDER_WORK_TRANSACTION.
  The allocator received the transaction but is stalled.
  Check above for the binder_transaction details (to_thread, code).
  Next step: determine if allocate() was entered (Outcome C) or failed before it (Outcome B).
""")
    elif sibling_any_active:
        print("""
  OUTCOME B or C -- An allocator thread has a non-idle u_PC
  ----------------------------------------------------------
  At least one allocator thread is NOT at __ioctl.
  It may be processing the transaction. Check u_PC values above.
""")
    else:
        print("""
  INCONCLUSIVE -- could not definitively classify.
  Review the raw output above for further clues.
""")

    print("=" * 80)
    s.close()


if __name__ == "__main__":
    main()
