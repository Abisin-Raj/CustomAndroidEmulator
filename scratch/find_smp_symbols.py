import struct
import capstone
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM

kernel_img = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\kernel.img"

print("=== Analyzing kernel.img for SMP Boot Functions ===", flush=True)

with open(kernel_img, "rb") as f:
    kernel_data = f.read()

print(f"Kernel Size: {len(kernel_data)} bytes ({len(kernel_data)/(1024*1024):.2f} MB)", flush=True)

# Search for PSCI / SMC CPU_ON call signature: 0xC4000003 or 0x84000003
psci_cpu_on_64 = 0xC4000003
psci_cpu_on_32 = 0x84000003

matches_64 = []
matches_32 = []

for i in range(0, len(kernel_data) - 4, 4):
    val = struct.unpack('<I', kernel_data[i:i+4])[0]
    if val == psci_cpu_on_64:
        matches_64.append(i)
    elif val == psci_cpu_on_32:
        matches_32.append(i)

print(f"Found {len(matches_64)} occurrences of PSCI CPU_ON 64-bit (0xC4000003):", flush=True)
for off in matches_64[:5]:
    va = 0xFFFFFF8008000000 + off
    pa = 0x40080000 + off
    print(f"  File Offset: 0x{off:x} | VA: 0x{va:016x} | PA: 0x{pa:016x}", flush=True)

print(f"Found {len(matches_32)} occurrences of PSCI CPU_ON 32-bit (0x84000003):", flush=True)
for off in matches_32[:5]:
    va = 0xFFFFFF8008000000 + off
    pa = 0x40080000 + off
    print(f"  File Offset: 0x{off:x} | VA: 0x{va:016x} | PA: 0x{pa:016x}", flush=True)

md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)

# Disassemble instructions around first PSCI CPU_ON match
if matches_64 or matches_32:
    target_off = (matches_64 + matches_32)[0]
    start_off = max(0, target_off - 64)
    end_off = min(len(kernel_data), target_off + 64)
    
    print(f"\n=== Disassembly of PSCI CPU_ON Invocation @ Offset 0x{target_off:x} ===", flush=True)
    chunk = kernel_data[start_off:end_off]
    for insn in md.disasm(chunk, 0x40080000 + start_off):
        marker = " <=== PSCI CPU_ON PARAMETER" if (insn.address - 0x40080000) == target_off else ""
        print(f"0x{insn.address:016x}: {insn.mnemonic}\t{insn.op_str}{marker}", flush=True)
