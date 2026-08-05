import sys
import pip


try:
    import capstone
    print("capstone is installed", flush=True)
except ImportError:
    import subprocess
    subprocess.run([sys.executable, "-m", "pip", "install", "capstone"])
    import capstone

from capstone import *

opcodes = [
    (0x0000000041c9c310, 0xd5182000), # msr ttbr0_el1
    (0x0000000041c9c314, 0xd5182021), # msr ttbr1_el1
    (0x0000000041c9c318, 0xd508871f), # tlbi vmalle1
    (0x0000000041c9c31c, 0xd5033fdf), # isb
    (0x0000000041c9c320, 0xd5181000), # msr sctlr_el1, x0
    (0x0000000041c9c324, 0xd5033fdf), # isb
    (0x0000000041c9c328, 0xd61f0060), # br x3
]


md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)

print("=== ARM64 Disassembly of Fault Block ===", flush=True)
for addr, op in opcodes:
    code_bytes = op.to_bytes(4, byteorder='little')
    for insn in md.disasm(code_bytes, addr):
        marker = " <== FAULTING INSTRUCTION" if addr == 0x41c9c1fc else ""
        print(f"0x{insn.address:016x}: {insn.mnemonic}\t{insn.op_str}{marker}", flush=True)
