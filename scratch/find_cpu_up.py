kernel_img = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\kernel.img"

with open(kernel_img, "rb") as f:
    kernel_data = f.read()

keywords = [
    b"failed to",
    b"online",
    b"secondary",
    b"smp_"
]

print("=== Searching kernel.img for SMP Substrings ===", flush=True)
for kw in keywords:
    pos = 0
    count = 0
    while count < 5:
        idx = kernel_data.find(kw, pos)
        if idx == -1:
            break
        va = 0xFFFFFF8008000000 + idx
        snippet = kernel_data[max(0, idx-10):min(len(kernel_data), idx+30)]
        print(f"  Found '{kw.decode()}' @ Offset 0x{idx:x} | VA: 0x{va:016x} | Snippet: {snippet}", flush=True)
        pos = idx + len(kw)
        count += 1

