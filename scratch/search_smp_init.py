map_file = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\reference\system_maps\System.map.stub"

print("=== All SMP / CPU Symbols in System.map.stub ===", flush=True)
with open(map_file, "r") as f:
    for line in f:
        parts = line.strip().split()
        if len(parts) >= 3:
            addr, t, name = parts[0], parts[1], parts[2]
            if "smp" in name.lower() or "cpu" in name.lower() or "boot" in name.lower():
                print(f"  0x{addr}: {t} {name}", flush=True)
