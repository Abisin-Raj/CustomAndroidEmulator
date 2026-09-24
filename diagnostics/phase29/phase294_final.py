#!/usr/bin/env python3
# phase294_final.py  -- Phase 29.4 final: catch PID 434 in ioctl, inspect binder_procs
#
# KEY FINDINGS SO FAR:
#   - PID 434 oscillates between ioctl(fd=9, BINDER_WRITE_READ) and futex
#   - When in ioctl: PC=0x75732afd78, x8=29
#   - When in futex: PC=0x75732518a0, x8=98
#   - file+0xf8 is the likely private_data (binder_proc) for fd[9]
#   - PID 255 (allocator) ALWAYS at ioctl receive on fd=4, never processes
#
# GOALS:
#   1. Poll until PID 434 is in ioctl mode → decode BWR (find target handle)
#   2. Read fd[9] private_data at file+0xf8 → inspect BA's hwbinder binder_proc
#   3. Read fd[4] private_data for PID 255 → inspect allocator's binder_proc
#   4. Produce definitive Outcome A sub-cause

import vm_utils
import time
import sys
sys.stdout.reconfigure(line_buffering=True)

TTBR1_PA        = 0x43006000
BA_MAIN_TASK_VA = 0xffffffc0ee51ec00
PID434_TASK_VA  = 0xffffffc0f1bc8d80
ALLOC_TASK_VA   = 0xffffffc0ee518000

PC_IOCTL = 0x75732afd78   # PID 434 in ioctl mode
PC_FUTEX = 0x75732518a0   # PID 434 in futex mode
FD9_FILE = 0xffffffc0ee716448   # fd[9] struct file* in BA process (from dump)

def kpa(va): return vm_utils.kva_to_phys(va)
def r64(s,f,pa): return vm_utils.read_phys_u64(s,f,pa)
def r32(s,f,pa): return vm_utils.read_phys_u32(s,f,pa)
def is_kva(v): return 0xffffff8000000000 <= v <= 0xfffffffe00000000

def walk_kpt(s,f,va): return vm_utils.walk_kernel_pt(s,f,TTBR1_PA,va)
def walk_user_pt(s,f,pgd_pa,va):
    va &= 0x00ffffffffffffff
    for shift, mask, align in [(30, 0x1ff, 0x3fffffff), (21, 0x1ff, 0x1fffff)]:
        idx = (va >> shift) & mask
        entry = r64(s,f,pgd_pa + idx*8)
        if (entry & 3) == 1:
            return (entry & 0x0000fffffffff000 & ~align) | (va & align)
        if (entry & 3) != 3: return None
        pgd_pa = entry & 0x0000fffffffff000
    l3_idx = (va >> 12) & 0x1ff
    l3e = r64(s,f,pgd_pa + l3_idx*8)
    if (l3e & 3) != 3: return None
    return (l3e & 0x0000fffffffff000) | (va & 0xfff)

def read_pt_regs(s,f,task_pa):
    sv = r64(s,f,task_pa+0x60)
    if not sv: return {}
    pa = walk_kpt(s,f,sv+0x3000)
    if not pa: return {}
    base = pa+0xec0
    regs = {'pc': r64(s,f,base+0x100), 'x8': r64(s,f,base+8*8)}
    for i in range(31): regs[f'x{i}'] = r64(s,f,base+i*8)
    regs['sp'] = r64(s,f,base+0xf8)
    return regs

def get_pgd(s,f,task_pa):
    mm = r64(s,f,task_pa+0x470)
    if not is_kva(mm): return None
    pgd = r64(s,f,kpa(mm)+0x48)
    if not is_kva(pgd): return None
    return kpa(pgd)

def safe_str(data): return ''.join(chr(b) if 32<=b<127 else '.' for b in data)

def read_user_bytes(s,f,pgd_pa,va,count):
    count = min(count,2048)
    result = bytearray()
    while len(result) < count:
        off = len(result)
        pa = walk_user_pt(s,f,pgd_pa,va+off)
        if pa is None:
            result.extend(b'\x00'*8)
            continue
        chunk = min(8, count-off)
        v = r64(s,f,pa)
        for b in range(chunk): result.append((v>>(b*8))&0xff)
    return bytes(result[:count])

def decode_bwr_and_txn(s,f,pgd_pa,bwr_va):
    bwr_pa = walk_user_pt(s,f,pgd_pa,bwr_va)
    if not bwr_pa:
        print(f"    ERROR: cant map BWR @ 0x{bwr_va:x}")
        return
    ws = r64(s,f,bwr_pa+0); wc = r64(s,f,bwr_pa+8); wb = r64(s,f,bwr_pa+16)
    rs = r64(s,f,bwr_pa+24); rc = r64(s,f,bwr_pa+32); rb = r64(s,f,bwr_pa+40)
    print(f"    BWR: write_size={ws}, consumed={wc}, buf=0x{wb:x}")
    print(f"         read_size={rs},  consumed={rc}, buf=0x{rb:x}")
    if ws == 0:
        print("    write_size=0 => PID 434 is in RECEIVE mode (looper, waiting for inbound txns)")
        print("    => PID 434 is a hwbinder SERVER thread, not sending to allocator!")
        return
    print(f"    write_size={ws} => PID 434 is SENDING a transaction")
    data = read_user_bytes(s,f,pgd_pa,wb,min(int(ws),256))
    pos = 0
    while pos+4 <= len(data):
        cmd = int.from_bytes(data[pos:pos+4],'little'); pos+=4
        if cmd == 0x40406300:  # BC_TRANSACTION
            print(f"\n    BC_TRANSACTION at write_buf+0x{pos-4:x}:")
            if pos+64 <= len(data):
                handle   = int.from_bytes(data[pos:pos+4],'little')
                code     = int.from_bytes(data[pos+16:pos+20],'little')
                flags    = int.from_bytes(data[pos+20:pos+24],'little')
                spid     = int.from_bytes(data[pos+24:pos+28],'little')
                dsz      = int.from_bytes(data[pos+32:pos+40],'little')
                dbuf     = int.from_bytes(data[pos+48:pos+56],'little')
                print(f"      target.handle = {handle}  <- hwbinder handle for target service")
                print(f"      code          = 0x{code:x}  <- HIDL method index")
                print(f"      flags         = 0x{flags:x}")
                print(f"      sender_pid    = {spid}")
                print(f"      data_size     = {dsz}")
                if dbuf and dsz>0:
                    pdata = read_user_bytes(s,f,pgd_pa,dbuf,min(int(dsz),96))
                    print(f"      Parcel data (96 bytes):")
                    for j in range(0,len(pdata),16):
                        c=pdata[j:j+16]; h=' '.join(f'{b:02x}' for b in c)
                        print(f"        +0x{j:02x}: {h:<48s}  {safe_str(c)}")
                    # Try to extract UTF-16 interface name
                    if len(pdata)>=8:
                        sm = int.from_bytes(pdata[0:4],'little')
                        nlen = int.from_bytes(pdata[4:8],'little')
                        print(f"      StrictMode=0x{sm:x}, name_len={nlen}")
                        if 0 < nlen <= 80:
                            chars=[]
                            for i in range(nlen):
                                off = 8+i*2
                                if off+2 <= len(pdata):
                                    ch = int.from_bytes(pdata[off:off+2],'little')
                                    chars.append(chr(ch) if 32<=ch<127 else '?')
                            print(f"      Interface: {''.join(chars)}")
            break
        elif cmd in (0x3f0b6200, 0x3f0c6200, 0x3f0d6200):
            pos+=4  # skip BC_INCREFS/ACQUIRE/RELEASE
        else:
            print(f"    cmd=0x{cmd:08x} (unknown), stopping"); break

def inspect_binder_proc_at(s,f,bp_va,label):
    """
    Given a candidate binder_proc VA, find:
    1. pid field (scan 0x28..0xd0 for plausible pid)
    2. threads rb_root (at +0x10)
    3. todo list_head (scan for non-empty lists)
    """
    bp_pa = kpa(bp_va)
    print(f"\n  [{label}] binder_proc candidate @ 0x{bp_va:x} (PA 0x{bp_pa:x})")

    # Find pid
    pid_val = 0; pid_off = 0
    for off in range(0x28, 0xd4, 4):
        v = r32(s,f,bp_pa+off)
        if 50 <= v <= 700:
            pid_val = v; pid_off = off
            print(f"    pid-like field: +0x{off:x} = {v}")

    # Threads RB-tree at +0x10
    threads_root = r64(s,f,bp_pa+0x10)
    print(f"    proc->threads rb_root = 0x{threads_root:x}")
    if is_kva(threads_root):
        # Walk threads
        stack = [threads_root]; visited = set()
        while stack and len(visited)<32:
            node_va = stack.pop()
            if node_va in visited or not is_kva(node_va): continue
            visited.add(node_va)
            node_pa = kpa(node_va)
            rb_right = r64(s,f,node_pa+8); rb_left = r64(s,f,node_pa+16)
            # binder_thread.rb_node at +0x08 → thread base = node-0x08
            thr_va = node_va-0x08; thr_pa = kpa(thr_va)
            thr_pid = r32(s,f,thr_pa+0x20)
            txn_stk = r64(s,f,thr_pa+0x28)
            looper  = r32(s,f,thr_pa+0x40)
            print(f"    thread pid={thr_pid} va=0x{thr_va:x} txn_stack=0x{txn_stk:x} looper=0x{looper:x}")
            if is_kva(txn_stk):
                tx_pa = kpa(txn_stk)
                from_t = r64(s,f,tx_pa+0x18); to_p = r64(s,f,tx_pa+0x28)
                code   = r32(s,f,tx_pa+0x40); flags = r32(s,f,tx_pa+0x44)
                print(f"      LIVE TXN: from=0x{from_t:x} to_proc=0x{to_p:x} code=0x{code:x} flags=0x{flags:x}")
            if is_kva(rb_right): stack.append(rb_right)
            if is_kva(rb_left):  stack.append(rb_left)
    else:
        print(f"    threads rb_root is invalid/empty")

    # Scan for non-empty list_heads (proc->todo)
    print(f"    Scanning proc for non-empty list_heads (todo, etc)...")
    non_empty = []
    for off in range(0x40, 0x280, 8):
        nxt = r64(s,f,bp_pa+off); prv = r64(s,f,bp_pa+off+8)
        self_va = bp_va+off
        if nxt == self_va and prv == self_va: continue  # empty list
        if is_kva(nxt) and is_kva(prv) and nxt != 0:
            work_va = nxt-0x08
            if is_kva(work_va):
                wtype = r32(s,f,kpa(work_va))
                WTYPES = {0:'NOOP',1:'TRANSACTION',2:'TXN_COMPLETE',3:'ERROR',4:'DEAD_BINDER'}
                wname = WTYPES.get(wtype, f'type_{wtype}')
                non_empty.append((off, nxt, wtype, wname))
                print(f"    NON-EMPTY at +0x{off:x}: work @ 0x{work_va:x} type={wname}")
                if wtype == 1:
                    tx_pa = kpa(work_va-0x08)
                    from_t = r64(s,f,tx_pa+0x18); to_p = r64(s,f,tx_pa+0x28)
                    code   = r32(s,f,tx_pa+0x40)
                    print(f"      TRANSACTION: from=0x{from_t:x} to_proc=0x{to_p:x} code=0x{code:x}")
    if not non_empty:
        print(f"    All list_heads empty -> proc->todo is EMPTY, no pending work")

def main():
    s,f = vm_utils.connect_qmp()

    print("="*70)
    print("PHASE 29.4 FINAL: CATCH PID 434 IN IOCTL + INSPECT BINDER_PROCS")
    print("="*70)

    print("\n[1] Restoring CP-02, running 20s ...")
    vm_utils.qmp_hmp(s,f,"loadvm cp02_rtr_token")
    vm_utils.qmp_hmp(s,f,"c"); time.sleep(20)
    vm_utils.qmp_hmp(s,f,"stop")
    print("    VM stopped.")

    ba_main_pa = kpa(BA_MAIN_TASK_VA)
    p434_pa    = kpa(PID434_TASK_VA)
    pgd_pa     = get_pgd(s,f,ba_main_pa)
    print(f"    PGD = 0x{pgd_pa:x}")

    # Poll for PID 434 in ioctl mode (up to 30 additional seconds)
    print("\n[2] Polling until PID 434 is in ioctl mode (PC=0x75732afd78)...")
    regs434 = {}
    for attempt in range(31):
        regs = read_pt_regs(s,f,p434_pa)
        pc434 = regs.get('pc',0)
        x8_434 = regs.get('x8',0)
        print(f"    attempt {attempt}: PC=0x{pc434:x}, x8={x8_434} ({'ioctl' if x8_434==29 else 'futex' if x8_434==98 else 'other'})")
        if x8_434 == 29:
            regs434 = regs
            print(f"    --> CAUGHT IN IOCTL MODE!")
            break
        if attempt < 30:
            vm_utils.qmp_hmp(s,f,"c"); time.sleep(1); vm_utils.qmp_hmp(s,f,"stop")
    else:
        print("    WARNING: PID 434 never caught in ioctl after 30 attempts")

    if regs434:
        print(f"\n[3] Decoding PID 434's BWR (x2=0x{regs434.get('x2',0):x})...")
        x2 = regs434.get('x2',0) & 0x0000ffffffffffff
        if x2 and pgd_pa:
            decode_bwr_and_txn(s,f,pgd_pa,x2)
        else:
            print(f"    x2=0x{x2:x} (zero or invalid) - cannot decode BWR")

    # Inspect BA hwbinder binder_proc via fd[9] private_data at file+0xf8
    print("\n[4] Inspecting BA process hwbinder binder_proc (fd[9], file+0xf8)...")
    fd9_file_pa = kpa(FD9_FILE)
    bp_va_ba = r64(s,f,fd9_file_pa+0xf8)  # private_data at file+0xf8
    print(f"    fd[9] file+0xf8 = 0x{bp_va_ba:x}")
    if is_kva(bp_va_ba):
        inspect_binder_proc_at(s,f,bp_va_ba,"BA hwbinder binder_proc")
    else:
        print(f"    NOT a KVA! private_data not at +0xf8 for fd[9]")
        # Try nearby offsets
        for try_off in range(0xb0,0x110,8):
            v = r64(s,f,fd9_file_pa+try_off)
            if is_kva(v):
                print(f"    KVA at file+0x{try_off:x}: 0x{v:x}")

    # Inspect PID 255 allocator binder_proc via fd[4] private_data at file+0xf8
    print("\n[5] Inspecting PID 255 allocator binder_proc (fd[4], file+0xf8)...")
    alloc_pa = kpa(ALLOC_TASK_VA)
    # Find fd array for PID 255
    fd_arr_pa_255 = None
    for off in range(0x4f0,0x5a0,8):
        v = r64(s,f,alloc_pa+off)
        if not is_kva(v): continue
        fdt = r64(s,f,kpa(v)+0x20)
        if not is_kva(fdt): continue
        mf = r32(s,f,kpa(fdt))
        if 4 <= mf <= 65536:
            farr = r64(s,f,kpa(fdt)+8)
            if is_kva(farr):
                fd_arr_pa_255 = kpa(farr)
                print(f"    Allocator fd_array at PA 0x{fd_arr_pa_255:x}, max_fds={mf}")
                break

    if fd_arr_pa_255:
        # fd[4] for allocator's /dev/hwbinder
        fp4 = r64(s,f,fd_arr_pa_255+4*8)
        print(f"    fd[4] struct file ptr = 0x{fp4:x}")
        if is_kva(fp4):
            fp4_pa = kpa(fp4)
            # Dump file struct around private_data
            print(f"    fd[4] file struct KVAs:")
            for off in range(0,0x108,8):
                v = r64(s,f,fp4_pa+off)
                if is_kva(v) and v not in (r64(s,f,fp4_pa+off-8) if off>=8 else 0,):
                    print(f"      file+0x{off:03x}: 0x{v:x}")
            bp_va_255 = r64(s,f,fp4_pa+0xf8)
            print(f"    fd[4] file+0xf8 = 0x{bp_va_255:x}")
            if is_kva(bp_va_255):
                inspect_binder_proc_at(s,f,bp_va_255,"PID 255 allocator binder_proc")
            else:
                print("    NOT a KVA at +0xf8, trying other offsets...")
                for try_off in range(0xa0,0x110,8):
                    v = r64(s,f,fp4_pa+try_off)
                    if is_kva(v) and not (v == r64(s,f,fp4_pa+try_off-8) if try_off>=8 else False):
                        print(f"    KVA at file+0x{try_off:x}: 0x{v:x}")

    # FINAL VERDICT
    print("\n"+"="*70)
    print("PHASE 29.4 FINAL VERDICT")
    print("="*70)
    if regs434:
        x2 = regs434.get('x2',0)
        if x2 == 0:
            print("\n  PID 434 x2=0 in ioctl => RECEIVE MODE (looper)")
            print("  => PID 434 is a hwbinder SERVER thread waiting for inbound calls")
            print("  => NOBODY is sending a transaction to PID 255 (allocator)")
            print()
            print("  ROOT CAUSE: ANGLE/SwiftShader's eglInitialize never makes")
            print("  the gralloc HIDL call because it's stuck earlier in init")
            print("  (all worker threads blocked on condition variables).")
        else:
            print(f"\n  PID 434 x2=0x{x2:x} in ioctl => SEND MODE")
            print("  => Decoded transaction above; check target.handle and outcome")
    else:
        print("\n  Could not catch PID 434 in ioctl. See binder_proc inspections above.")
    print("="*70)

    s.close()

if __name__ == "__main__":
    main()
