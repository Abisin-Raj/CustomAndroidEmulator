#!/usr/bin/env python3
# phase294_vma_binsearch.py -- VMA binary search via mm->mm_rb for BA process
#
# Goal: identify which library/region contains:
#   0x75732518a0  (futex stall PC for PIDs 417/428/429/434)
#   0x75732afd78  (ioctl stall PC for PIDs 246/410/411/434)
#   0x7362f75b78  (futex address PID 417 is waiting on)
#   0x73d2f7e774  (futex address PIDs 428/434 are waiting on)
#
# Also: scan fd[4] private_data for PID 255 using a broader offset scan.
#
# Uses mm->mm_rb (rb_tree) instead of mm->mmap (linked list) for O(log n) lookup.

import vm_utils
import time
import sys
sys.stdout.reconfigure(line_buffering=True)

TTBR1_PA        = 0x43006000
BA_MAIN_TASK_VA = 0xffffffc0ee51ec00
ALLOC_TASK_VA   = 0xffffffc0ee518000

def kpa(va):  return vm_utils.kva_to_phys(va)
def r64(s,f,pa): return vm_utils.read_phys_u64(s,f,pa)
def r32(s,f,pa): return vm_utils.read_phys_u32(s,f,pa)
def is_kva(v): return 0xffffff8000000000 <= v <= 0xfffffffe00000000
def walk_kpt(s,f,va): return vm_utils.walk_kernel_pt(s,f,TTBR1_PA,va)

def safe_str(data): return ''.join(chr(b) if 32<=b<127 else '.' for b in data)

def read_name_at_kva(s, f, name_kva, maxlen=64):
    """Read a null-terminated C string from kernel VA."""
    name_pa = kpa(name_kva)
    result = []
    for off in range(0, maxlen, 8):
        v = r64(s, f, name_pa + off)
        for shift in range(0, 64, 8):
            b = (v >> shift) & 0xff
            if b == 0: return ''.join(result)
            result.append(chr(b) if 32 <= b < 127 else '?')
    return ''.join(result)

def get_vma_filename(s, f, vma_pa):
    """Try to extract filename from vm_area_struct's vm_file pointer."""
    # Try vm_file at several offsets (depends on kernel config)
    # In Linux 5.10 ARM64, vm_file is typically around +0xa8..+0xc0
    for vm_file_off in range(0x90, 0xd0, 8):
        vm_file = r64(s, f, vma_pa + vm_file_off)
        if not is_kva(vm_file): continue
        # struct file.f_path.dentry at f+0x10 (or try f+0x18 depending on layout)
        for dentry_off in (0x10, 0x18, 0x08, 0x20):
            dentry = r64(s, f, kpa(vm_file) + dentry_off)
            if not is_kva(dentry): continue
            # d_name.name at dentry+0x28+8 = dentry+0x30 (len at +0x28)
            for name_ptr_off in (0x30, 0x28, 0x38):
                name_ptr = r64(s, f, kpa(dentry) + name_ptr_off)
                if not is_kva(name_ptr): continue
                name = read_name_at_kva(s, f, name_ptr)
                if 3 <= len(name) <= 60 and '.' in name:
                    return name
    return 'anon'

def vma_binary_search(s, f, mm_pa, target_va):
    """
    Binary search mm->mm_rb for VMA containing target_va.
    vm_area_struct layout (Linux 5.10 ARM64):
      +0x00: vm_start
      +0x08: vm_end
      +0x10: vm_next
      +0x18: vm_prev
      +0x20: vm_rb (rb_node: +0x00=parent_color, +0x08=rb_right, +0x10=rb_left)
      ...
      +0xa0..0xc0: vm_file
    rb_node embedded at vma+0x20, so: vma = rb_node_va - 0x20
    """
    # Get rb_root from mm->mm_rb (at mm+0x08)
    rb_root = r64(s, f, mm_pa + 0x08)   # mm_rb.rb_node
    
    if not is_kva(rb_root):
        # Try alternate offset (some kernels have mm_rb at different position)
        # Let's try mm+0x00 (mm->mmap, but that's the list not rb)
        # and compute rb_root differently
        print(f"    mm_rb.rb_node @ mm+0x08 = 0x{rb_root:x} (not KVA), trying mm+0x10...")
        rb_root = r64(s, f, mm_pa + 0x10)
        if not is_kva(rb_root):
            return None
    
    # rb_node is at vma+0x20
    VMA_RB_OFF = 0x20
    
    node = rb_root
    depth = 0
    while is_kva(node) and depth < 64:
        depth += 1
        node_pa = kpa(node)
        vma_va  = node - VMA_RB_OFF
        vma_pa  = kpa(vma_va)
        
        vm_start = r64(s, f, vma_pa + 0x00)
        vm_end   = r64(s, f, vma_pa + 0x08)
        
        if vm_start <= target_va < vm_end:
            return vma_va, vma_pa, vm_start, vm_end
        elif target_va < vm_start:
            node = r64(s, f, node_pa + 0x10)   # rb_left
        else:
            node = r64(s, f, node_pa + 0x08)   # rb_right
    
    # Fallback: try VMA_RB_OFF = 0x28
    print(f"    Binary search with vma_rb at +0x20 failed. Trying +0x28...")
    VMA_RB_OFF = 0x28
    node = rb_root
    depth = 0
    while is_kva(node) and depth < 64:
        depth += 1
        node_pa = kpa(node)
        vma_va  = node - VMA_RB_OFF
        vma_pa  = kpa(vma_va)
        vm_start = r64(s, f, vma_pa + 0x00)
        vm_end   = r64(s, f, vma_pa + 0x08)
        if vm_start <= target_va < vm_end:
            return vma_va, vma_pa, vm_start, vm_end
        elif target_va < vm_start:
            node = r64(s, f, node_pa + 0x10)
        else:
            node = r64(s, f, node_pa + 0x08)
    return None

def get_mm_pa(s, f, task_pa):
    mm_va = r64(s, f, task_pa + 0x470)
    if not is_kva(mm_va): return None
    return kpa(mm_va)

def read_pt_regs_all(s, f, task_pa):
    sv = r64(s, f, task_pa + 0x60)
    if not sv: return {}
    pa = walk_kpt(s, f, sv + 0x3000)
    if not pa: return {}
    base = pa + 0xec0
    regs = {}
    for i in range(31): regs[f'x{i}'] = r64(s, f, base + i*8)
    regs['sp']  = r64(s, f, base + 0xf8)
    regs['pc']  = r64(s, f, base + 0x100)
    return regs


ADDRESSES_TO_LOOKUP = [
    (0x75732518a0, "PID 417/428/434 FUTEX stall PC"),
    (0x75732afd78, "PID 246/410/411/434 IOCTL stall PC"),
    (0x7362f75b78, "PID 417 futex address (condvar)"),
    (0x73d2f7e774, "PID 428/434 futex address (condvar)"),
    (0x7442f8a770, "PID 429 futex address (condvar)"),
    (0x7573256184, "PID 417/434 LR (return addr from stall)"),
]

def main():
    s, f = vm_utils.connect_qmp()

    print("=" * 70)
    print("PHASE 29.4: VMA BINARY SEARCH - IDENTIFY ANGLE STALL LIBRARY")
    print("=" * 70)

    print("\n[1] Restoring CP-02, running 20s...")
    vm_utils.qmp_hmp(s, f, "loadvm cp02_rtr_token")
    vm_utils.qmp_hmp(s, f, "c"); time.sleep(20)
    vm_utils.qmp_hmp(s, f, "stop")
    print("    VM stopped.")

    ba_main_pa = kpa(BA_MAIN_TASK_VA)
    mm_pa = get_mm_pa(s, f, ba_main_pa)
    print(f"    BA mm PA = 0x{mm_pa:x}" if mm_pa else "    ERROR: no mm")

    # Verify mm->mm_rb.rb_node and mm->mmap
    mmap_head = r64(s, f, mm_pa + 0x00)
    mm_rb_node = r64(s, f, mm_pa + 0x08)
    print(f"    mm->mmap    = 0x{mmap_head:x} ({'KVA OK' if is_kva(mmap_head) else 'INVALID'})")
    print(f"    mm->mm_rb   = 0x{mm_rb_node:x} ({'KVA OK' if is_kva(mm_rb_node) else 'INVALID'})")

    # VMA binary search for each address
    print("\n[2] VMA binary search results:")
    print(f"  {'Address':<20s} {'Range':<40s} {'Offset':<12s} {'Library'}")
    print("  " + "-" * 110)

    for addr, desc in ADDRESSES_TO_LOOKUP:
        result = vma_binary_search(s, f, mm_pa, addr)
        if result:
            vma_va, vma_pa, vm_start, vm_end = result
            offset = addr - vm_start
            # Get filename
            fname = get_vma_filename(s, f, vma_pa)
            range_str = f"[0x{vm_start:x}, 0x{vm_end:x})"
            print(f"  0x{addr:x}   {range_str:<40s} +0x{offset:08x}  {fname}")
            print(f"    [{desc}]")
        else:
            print(f"  0x{addr:x}   NOT FOUND in VMA tree")
            print(f"    [{desc}]")

    # Also check PID 417's x2/x3 (futex expected value and timeout)
    print("\n[3] PID 417 full futex syscall args (x0-x5 from pt_regs)...")
    WORKER_TASK_VA = 0xffffffc0fb178000
    worker_pa = kpa(WORKER_TASK_VA)
    regs = read_pt_regs_all(s, f, worker_pa)
    if regs:
        print(f"    x0 (futex addr) = 0x{regs['x0']:x} -> 0x{regs['x0']&0xffffffffffff:x} (stripped tag)")
        print(f"    x1 (op)         = 0x{regs['x1']:x}  = FUTEX_WAIT_BITSET|PRIVATE(0x89)")
        print(f"    x2 (expected)   = 0x{regs['x2']:x}  (futex word expected value)")
        print(f"    x3 (timeout)    = 0x{regs['x3']:x}  ({'NULL = infinite wait' if regs['x3']==0 else 'has timeout'})")
        print(f"    x4 (uaddr2)     = 0x{regs['x4']:x}")
        print(f"    x5 (bitset)     = 0x{regs['x5']:x}  ({'ALL_BITS=0xffffffff' if regs['x5']==0xffffffff else ''})")
        print(f"    x8 (sc#)        = {regs['x8']} ({'futex' if regs['x8']==98 else 'other'})")

    # Inspect PID 255 allocator - try wider scan for binder_proc
    print("\n[4] Locating PID 255 binder_proc in fd[4]...")
    alloc_pa = kpa(ALLOC_TASK_VA)
    alloc_mm_pa = get_mm_pa(s, f, alloc_pa)
    
    # Find fd array for PID 255
    fd_arr_pa_255 = None
    for off in range(0x4f0, 0x5a0, 8):
        v = r64(s, f, alloc_pa + off)
        if not is_kva(v): continue
        fdt = r64(s, f, kpa(v) + 0x20)
        if not is_kva(fdt): continue
        mf = r32(s, f, kpa(fdt))
        if 4 <= mf <= 65536:
            farr = r64(s, f, kpa(fdt) + 8)
            if is_kva(farr):
                fd_arr_pa_255 = kpa(farr)
                break

    if fd_arr_pa_255:
        fp4 = r64(s, f, fd_arr_pa_255 + 4*8)
        if is_kva(fp4):
            fp4_pa = kpa(fp4)
            # WIDE SCAN: try ALL KVA-containing offsets in file struct (0x00 to 0x180)
            # For each, check if it contains pid=255 at offsets 0x28..0xd0
            print(f"    fd[4] file @ 0x{fp4:x}, PA=0x{fp4_pa:x}")
            print(f"    Scanning all KVA pointers in file struct for binder_proc(pid=255)...")
            found_bp = None
            for file_off in range(0x00, 0x180, 8):
                priv = r64(s, f, fp4_pa + file_off)
                if not is_kva(priv): continue
                # Skip if it's the same address range as the file itself
                if fp4 - 0x1000 < priv < fp4 + 0x1000: continue
                priv_pa = kpa(priv)
                # Scan for pid=255 at various offsets in the candidate struct
                for p_off in range(0x28, 0xe0, 4):
                    try:
                        pval = r32(s, f, priv_pa + p_off)
                    except: continue
                    if pval == 255:
                        print(f"    fd[4] file+0x{file_off:x} -> 0x{priv:x}: pid=255 at +0x{p_off:x} *** BINDER_PROC FOUND!")
                        found_bp = (priv, file_off, p_off)
                        break
                if found_bp: break
            
            if not found_bp:
                print(f"    Could not find binder_proc(pid=255) via fd[4] file scan.")
                # Print all KVA pointers found
                print(f"    All KVA pointers in fd[4] file struct (non-adjacent range):")
                prev_v = 0
                for file_off in range(0x00, 0x180, 8):
                    v = r64(s, f, fp4_pa + file_off)
                    if is_kva(v) and v != prev_v:
                        print(f"      file+0x{file_off:03x}: 0x{v:x}")
                    prev_v = v

    # SUMMARY 
    print("\n" + "="*70)
    print("PHASE 29.4 VMA SUMMARY")
    print("="*70)
    print()
    print("  Key question: which library is 0x75732518a0 in?")
    print("  => This identifies WHERE in ANGLE/Pastel the stall occurs")
    print()
    print("  Key question: is 0x7362f75b78 in heap, mmap, or BSS?")
    print("  => Confirms this is a condvar/mutex in a shared library or heap")
    print()
    print("  Next steps:")
    print("  - If 0x75732518a0 is in libEGL.so / libGLES_swiftshader.so / etc.:")
    print("    => EGL init is stuck in a condvar wait within the EGL implementation")
    print("  - If 0x75732afd78 is in libbinder_ndk.so / libhidl*.so:")
    print("    => The ioctl is a Binder receive (looper), not a gralloc send")
    print("  - Root cause may be: ANGLE Vulkan backend trying to enumerate GPUs")
    print("    that don't exist in QEMU, causing an eternal wait")
    print("="*70)

    s.close()

if __name__ == "__main__":
    main()
