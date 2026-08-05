import struct
import os

boot_img_path = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\build\launcher\roms\PixelExperience_violet-13.0-20240127-0711-OFFICIAL\boot.img"
output_kernel_path = r"C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\scratch\kernel.img"

with open(boot_img_path, "rb") as f:
    hdr = f.read(4096)
    magic = hdr[:8]
    print(f"Magic: {magic}")
    
    kernel_size = struct.unpack("<I", hdr[8:12])[0]
    kernel_addr = struct.unpack("<I", hdr[12:16])[0]
    ramdisk_size = struct.unpack("<I", hdr[16:20])[0]
    page_size = struct.unpack("<I", hdr[36:40])[0]
    
    print(f"Kernel Size:  {kernel_size} bytes (0x{kernel_size:x})")
    print(f"Kernel Addr:  0x{kernel_addr:x}")
    print(f"Ramdisk Size: {ramdisk_size} bytes (0x{ramdisk_size:x})")
    print(f"Page Size:    {page_size} bytes")

    # Read kernel bytes starting at offset page_size
    f.seek(page_size)
    kernel_data = f.read(kernel_size)
    
    with open(output_kernel_path, "wb") as kf:
        kf.write(kernel_data)
        
    print(f"Successfully extracted kernel image to {output_kernel_path} ({len(kernel_data)} bytes)")
