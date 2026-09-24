import vm_utils
import socket
import struct
import time

s_qmp, f_qmp = vm_utils.connect_qmp()
print('Restoring CP-02...')
vm_utils.qmp_hmp(s_qmp, f_qmp, 'loadvm cp02_rtr_token')

gdb = socket.socket()
gdb.connect(('127.0.0.1', 1776))
gdb.sendall(b'+')

def gdb_cmd(cmd, timeout=5.0):
    csum = sum(cmd.encode('latin1')) % 256
    gdb.sendall(f"${cmd}#{csum:02x}".encode('latin1'))
    gdb.settimeout(timeout)
    while True:
        try:
            c = gdb.recv(1)
        except socket.timeout:
            return None
        if not c: return None
        if c == b'$':
            res = bytearray(b'$')
            while True:
                ch = gdb.recv(1)
                res.extend(ch)
                if ch == b'#':
                    res.extend(gdb.recv(2))
                    gdb.sendall(b'+')
                    return bytes(res)

def get_regs():
    raw = gdb_cmd('g')
    if not raw or b'$' not in raw: return []
    try:
        hex_d = raw.split(b'$')[1].split(b'#')[0].decode('ascii')
        return [struct.unpack('<Q', bytes.fromhex(hex_d[i:i+16]))[0] for i in range(0, len(hex_d)-15, 16)]
    except:
        return []

# Step past initial pause (0x756f4ab30c)
gdb_cmd('s')

# Set BP at CALL_EGL_INIT (0x756f4aba5c)
gdb_cmd('Z0,756f4aba5c,4')
csum = sum(b'c') % 256
gdb.sendall(f"$c#{csum:02x}".encode('latin1'))
pkt = gdb_cmd('?', timeout=10.0)
print('Stop at CALL_EGL_INIT:', pkt)
regs = get_regs()
print(f'Hit CALL_EGL_INIT: PC=0x{regs[32]:x}, LR=0x{regs[30]:x}, SP=0x{regs[31]:x}')
print(f'  X0 (dpy)=0x{regs[0]:x}, X1=0x{regs[1]:x}, X2=0x{regs[2]:x}')

# Remove BP at 0x756f4aba5c
gdb_cmd('z0,756f4aba5c,4')

# Also set BP at RET_EGL_INIT (0x756f4aba60)
gdb_cmd('Z0,756f4aba60,4')

# Step 1 instruction to execute 'bl #0x756f4b4090'
print('Single-stepping the bl instruction...')
s_pkt = gdb_cmd('s')
regs = get_regs()
print(f'After step: PC=0x{regs[32]:x}, LR=0x{regs[30]:x}, SP=0x{regs[31]:x}')

# Now let us run for 2s at a time and inspect PC of PID 417
print('\nSampling execution post-CALL_EGL_INIT...')
for i in range(1, 15):
    # Resume VM via c
    csum = sum(b'c') % 256
    gdb.sendall(f"$c#{csum:02x}".encode('latin1'))
    
    # Wait for either BP hit or timeout
    try:
        gdb.settimeout(2.0)
        pkt = gdb.recv(64)
        if pkt and (b'$T' in pkt or b'$S' in pkt):
            regs = get_regs()
            print(f'[{i*2.0:.1f}s] BP HIT! PC=0x{regs[32]:x}')
            if regs[32] == 0x756f4aba60:
                print('SUCCESS: RET_EGL_INIT HIT!')
                print(f'W0 (return code) = {regs[0]}')
                break
    except socket.timeout:
        # Pause VM to check thread state
        vm_utils.qmp_hmp(s_qmp, f_qmp, 'stop')
        regs = get_regs()
        pc = regs[32] if regs and len(regs)>32 else 0
        sp = regs[31] if regs and len(regs)>31 else 0
        lr = regs[30] if regs and len(regs)>30 else 0
        print(f'[{i*2.0:.1f}s] Sample: current CPU#0 PC=0x{pc:x}, LR=0x{lr:x}, SP=0x{sp:x}')

gdb_cmd('z0,756f4aba60,4')
gdb.close()
s_qmp.close()
