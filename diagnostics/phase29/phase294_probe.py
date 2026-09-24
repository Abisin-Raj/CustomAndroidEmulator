#!/usr/bin/env python3
# phase294_probe.py  -- Phase 29.4 diagnostic dump
#
# Goals:
#   1. Read u_PC of BA worker (417) and ALL allocator threads at multiple
#      time points (t=0, 10, 20, 30, 40, 50, 60s) to find the stall window.
#   2. Identify what 0x7077057d78 and 0x75732518a0 are (which library + offset).
#   3. Dump ALL fd private_data fields for BA main + allocator, scanning
#      multiple possible binder_proc.pid offsets to find the right one.
#
# Key corrections from previous runs:
#   - files_struct at task+0x570 (scan 0x4f0-0x580)
#   - pt_regs PC at: walk_kernel_pt(kstack+0x3000) -> pa + 0xec0 + 0x100

import vm_utils
import time
import sys
sys.stdout.reconfigure(line_buffering=True)

TTBR1_PA        = 0x43006000
BA_MAIN_TASK_VA = 0xffffffc0ee51ec00   # PID 246 (BA main)
WORKER_TASK_VA  = 0xffffffc0fb178000   # PID 417 (BA worker)
ALLOC_TASK_VA   = 0xffffffc0ee518000   # PID 255
ALLOC_THREADS   = [(302,0xffffffc0ea81b600),(303,0xffffffc0ea81c380),(313,0xffffffc0ea86b600)]

def kpa(va): return vm_utils.kva_to_phys(va)
def r64(s,f,pa): return vm_utils.read_phys_u64(s,f,pa)
def r32(s,f,pa): return vm_utils.read_phys_u32(s,f,pa)
def is_kva(v): return 0xffffff8000000000 <= v <= 0xfffffffe00000000

def read_pc(s,f,task_pa):
    sv = r64(s,f,task_pa+0x60)
    if not sv: return 0
    pa = vm_utils.walk_kernel_pt(s,f,TTBR1_PA,sv+0x3000)
    if not pa: return 0
    return r64(s,f,pa+0xec0+0x100)

def get_fd_array(s,f,task_pa):
    # scan 0x4f0 to 0x580 for files_struct pointer
    for off in range(0x4f0,0x590,8):
        v = r64(s,f,task_pa+off)
        if not is_kva(v): continue
        fdt = r64(s,f,kpa(v)+0x20)
        if not is_kva(fdt): continue
        mf = r32(s,f,kpa(fdt))
        if 4 <= mf <= 65536:
            farr = r64(s,f,kpa(fdt)+8)
            if is_kva(farr):
                return kpa(farr), mf, off
    return None, 0, 0

def dump_fd_table(s,f,fd_arr_pa,max_fds,label):
    """Dump first 32 fds, showing private_data and scanning for pid-like values."""
    print(f"\n  --- fd table dump for {label} ---")
    limit = min(max_fds,32)
    for fd in range(limit):
        file_ptr = r64(s,f,fd_arr_pa+fd*8)
        if not is_kva(file_ptr): continue
        file_pa = kpa(file_ptr)
        # struct file: f_path.dentry at +0x10 (->d_name.name to get filename)
        dentry = r64(s,f,file_pa+0x10)
        fname = '?'
        if is_kva(dentry):
            # d_name (qstr) at dentry+0x28: hash/len at +0, name ptr at +0x8
            name_ptr = r64(s,f,kpa(dentry)+0x28+8)  # &d_name.name
            if is_kva(name_ptr):
                # Read up to 32 chars
                res = vm_utils.qmp_hmp(s,f,f"xp /32c 0x{kpa(name_ptr):x}")
                chars=[]
                for l in res.splitlines():
                    if ':' not in l: continue
                    for c in l.split(':',1)[1].strip().split():
                        c=c.strip("'")
                        if c in (r'\x00',r'\0','\\000'): break
                        if len(c)==1: chars.append(c)
                        elif c.startswith(r'\x'):
                            try: chars.append(chr(int(c[2:],16)))
                            except: pass
                    if r'\x00' in res or '\\000' in res: break
                fname = ''.join(chars[:24])

        # Scan private_data offsets 0x98-0xd8
        privs = {}
        for pd_off in range(0x98,0xe0,8):
            pv = r64(s,f,file_pa+pd_off)
            if is_kva(pv):
                privs[pd_off] = pv

        if privs or 'hwbinder' in fname or 'binder' in fname:
            pstr = ', '.join(f"+0x{o:x}=0x{v:x}" for o,v in privs.items())
            print(f"    fd[{fd:2d}] '{fname:24s}' private_data_scan: {pstr}")

            # For each private_data candidate, scan for pid values
            for pd_off, pv in privs.items():
                pv_pa = kpa(pv)
                # Scan offsets 0x28..0xc0 for pid-like u32
                pids_found = {}
                for p_off in range(0x28,0xc8,4):
                    pval = r32(s,f,pv_pa+p_off)
                    if 100 <= pval <= 500:  # plausible PID range
                        pids_found[p_off] = pval
                if pids_found:
                    pid_str = ', '.join(f"+0x{o:x}={v}" for o,v in pids_found.items())
                    print(f"           -> priv@+0x{pd_off:x}=0x{pv:x}: pids={pid_str}")

def resolve_pc(s,f,task_pa,pc):
    """Try to identify which VMA the PC is in."""
    mm_va = r64(s,f,task_pa+0x470)
    if not is_kva(mm_va): return f"0x{pc:x} (no mm)"
    mm_pa = kpa(mm_va)
    cur_vma = r64(s,f,mm_pa)   # mm->mmap (first VMA)
    checked = 0
    while is_kva(cur_vma) and checked < 500:
        vma_pa = kpa(cur_vma)
        start = r64(s,f,vma_pa+0)
        end   = r64(s,f,vma_pa+8)
        if start <= pc < end:
            # Try to get filename
            vm_file = r64(s,f,vma_pa+0xa0)
            fname = 'anon'
            if is_kva(vm_file):
                dentry = r64(s,f,kpa(vm_file)+0x18)
                if is_kva(dentry):
                    name_ptr = r64(s,f,kpa(dentry)+0x28+8)
                    if is_kva(name_ptr):
                        res = vm_utils.qmp_hmp(s,f,f"xp /24c 0x{kpa(name_ptr):x}")
                        chars=[]
                        for l in res.splitlines():
                            if ':' not in l: continue
                            for c in l.split(':',1)[1].strip().split():
                                c=c.strip("'")
                                if c in (r'\x00',r'\0','\\000'): break
                                if len(c)==1: chars.append(c)
                            if r'\x00' in res or '\\000' in res: break
                        fname=''.join(chars[:32])
            return f"0x{pc:x} ({fname}+0x{pc-start:x})"
        cur_vma = r64(s,f,vma_pa+16)  # vm_area_struct.vm_next
        checked += 1
    return f"0x{pc:x} (not in VMA list)"

def main():
    s,f = vm_utils.connect_qmp()

    # === PART 1: Time-series sampling of u_PC ===
    print("="*70)
    print("PART 1: Time-series u_PC sampling after CP-02 restore")
    print("="*70)

    print("\nRestoring CP-02...")
    vm_utils.qmp_hmp(s,f,"loadvm cp02_rtr_token")

    # Read BASELINE (before resume)
    def sample_pcs(label):
        upc_417 = read_pc(s,f,kpa(WORKER_TASK_VA))
        upc_255 = read_pc(s,f,kpa(ALLOC_TASK_VA))
        idle_255 = (upc_255 == 0x7077008970)
        stall_417 = (upc_417 == 0x7077008970)
        print(f"  [{label:8s}] PID417=0x{upc_417:x} {'STALL' if stall_417 else ''}"
              f"  | PID255=0x{upc_255:x} {'IDLE' if idle_255 else 'ACTIVE'}")

    sample_pcs("t=0s")

    vm_utils.qmp_hmp(s,f,"c")
    t0 = time.time()

    for target_t in [5,10,15,20,25,30,40,50,60]:
        now = time.time()-t0
        if now < target_t:
            time.sleep(target_t - now)
        vm_utils.qmp_hmp(s,f,"stop")
        label = f"t={target_t}s"
        sample_pcs(label)
        if target_t < 60:
            vm_utils.qmp_hmp(s,f,"c")

    # === PART 2: Identify what the observed PCs are ===
    print("\n"+"="*70)
    print("PART 2: PC identification (which library+offset)")
    print("="*70)

    print("\nChecking PID 417 PC...")
    upc_417 = read_pc(s,f,kpa(WORKER_TASK_VA))
    resolved = resolve_pc(s,f,kpa(WORKER_TASK_VA),upc_417)
    print(f"  PID 417: {resolved}")

    print("\nChecking PID 255 PC...")
    upc_255 = read_pc(s,f,kpa(ALLOC_TASK_VA))
    resolved_255 = resolve_pc(s,f,kpa(ALLOC_TASK_VA),upc_255)
    print(f"  PID 255: {resolved_255}")

    for pid,va in ALLOC_THREADS:
        upc = read_pc(s,f,kpa(va))
        resolved = resolve_pc(s,f,kpa(va),upc)
        print(f"  PID {pid}: {resolved}")

    # === PART 3: fd table dump to find binder_proc ===
    print("\n"+"="*70)
    print("PART 3: fd table dump for BA main (246) and allocator (255)")
    print("="*70)

    ba_main_pa = kpa(BA_MAIN_TASK_VA)
    fd_arr_pa, max_fds, foff = get_fd_array(s,f,ba_main_pa)
    if fd_arr_pa:
        print(f"\nBA main (PID 246): files at task+0x{foff:x}, max_fds={max_fds}")
        dump_fd_table(s,f,fd_arr_pa,max_fds,"BA main PID 246")
    else:
        print("BA main: files_struct NOT FOUND (scan 0x4f0-0x590 found nothing)")

    alloc_pa = kpa(ALLOC_TASK_VA)
    fd_arr_pa2, max_fds2, foff2 = get_fd_array(s,f,alloc_pa)
    if fd_arr_pa2:
        print(f"\nAllocator (PID 255): files at task+0x{foff2:x}, max_fds={max_fds2}")
        dump_fd_table(s,f,fd_arr_pa2,max_fds2,"Allocator PID 255")
    else:
        print("Allocator: files_struct NOT FOUND")

    s.close()

if __name__ == "__main__":
    main()
