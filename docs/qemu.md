# QEMU Virtualization Integration

This document outlines how CustomAndroidEmulator configures and spawns the QEMU emulator backend on Windows.

---

## 1. Path Discovery Sequence

The `QemuDetector` checks the following sources in order to locate the target `qemu-system-aarch64` binary:
1. **User Custom Configuration**: Reads custom path written to `settings.json` via the Settings UI tab.
2. **Environment Path**: Queries the system `%PATH%` environment variable.
3. **MSYS2 Standard Directory**: Checks `C:\msys64\mingw64\bin\qemu-system-aarch64.exe`.
4. **Program Files Standard Directory**: Checks `C:\Program Files\qemu\qemu-system-aarch64.exe`.

---

## 2. Argument Construction Parameters

To boot generic Android kernels, QEMU is run with parameters mapped by `QemuArguments::build`:

| Flag | Purpose | Source / Mapping |
| --- | --- | --- |
| `-M` | Guest Machine Board Type | Profile `machine` parameter (e.g. `virt`) |
| `-cpu` | Virtual Processor Architecture | Profile `cpu` parameter (e.g. `cortex-a53`) |
| `-m` | RAM Memory size | Profile `memory` parameter (e.g. `1024M`) |
| `-kernel` | Kernel Binary Image File | Extracted temporary physical kernel |
| `-initrd` | Ramdisk Filesystem Image File | Extracted temporary physical ramdisk (if present) |
| `-serial` | Redirect QEMU serial output | Always set to `mon:stdio` for console piping |
| `-append` | Kernel Commandline options | Extracted boot image cmdline augmented by logger flags |
| `-nographic` | Disable display windows | Always enabled for CLI logs capture |

---

## 3. Kernel Commandline Augmentations

To force guest kernel prints to be emitted to QEMU's stdout (which our launchers capture in real-time), `KernelCommandLine::build` automatically appends these tracing variables if they are missing:
* **`console=ttyAMA0`**: Directs early console output stream to QEMU's serial virtual console.
* **`earlycon=pl011,0x09000000`**: Tells earlycon to map to standard PrimeCell PL011 UART registers on virt machine boards.
* **`printk.devkmsg=on`**: Enables system init logger output prints.
