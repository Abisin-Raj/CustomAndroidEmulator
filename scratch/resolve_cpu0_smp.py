import sys
import re

map_file = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\reference\system_maps\System.map.stub"

print("=== Reading System.map.stub for SMP Functions ===", flush=True)
try:
    with open(map_file, "r") as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 3:
                addr, t, name = parts[0], parts[1], parts[2]
                if any(kw in name.lower() for kw in ["smp", "cpu_up", "secondary", "online_mask", "boot_secondary"]):
                    print(f"  0x{addr}: {t} {name}", flush=True)
except Exception as e:
    print(f"Error reading System.map.stub: {e}", flush=True)
