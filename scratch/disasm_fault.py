import struct

opcodes = [
    (0x0000000041c9c1ec, 0xd280001e),
    (0x0000000041c9c1f0, 0x179073bc),
    (0x0000000041c9c1f4, 0xd5380701),
    (0x0000000041c9c1f8, 0xd35c7c22),
    (0x0000000041c9c1fc, 0xf100005f),
    (0x0000000041c9c200, 0x54000261),
    (0x0000000041c9c204, 0xd2800002),
    (0x0000000041c9c208, 0xb0006481),
]

# Write binary file for GDB disassembly
with open(r"c:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\fault_block.bin", "wb") as f:
    for addr, op in opcodes:
        f.write(struct.pack('<I', op))

print("Wrote fault_block.bin for disassembly", flush=True)
