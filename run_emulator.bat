@echo off
REM Start the Custom ARM64 Android Emulator on Windows

qemu-system-aarch64 ^
    -model virt ^
    -cpu max ^
    -smp 4 ^
    -m 4096 ^
    -machine type=virt,gic-version=3 ^
    -drive if=none,file=userdata.img,id=userdata,format=qcow2 ^
    -device virtio-blk-pci,drive=userdata ^
    -drive if=none,file=system.img,id=system,format=raw ^
    -device virtio-blk-pci,drive=system ^
    -netdev user,id=net0,hostfwd=tcp::5555-:5555 ^
    -device virtio-net-pci,netdev=net0 ^
    -device virtio-gpu-pci ^
    -display sdl,gl=on ^
    -device virtio-mouse-pci -device virtio-keyboard-pci

pause
