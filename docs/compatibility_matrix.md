# Emulator Compatibility Matrix

This document tracks overall guest custom ROM boot verification and compatibility levels.

---

## Compatibility Index

| ROM Package | Target Device | Android version | Virtual Profile | Active Status | Last Tested | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| `PixelExperience_violet-13.0` | `violet` | `13.0` | `violet` | `Kernel Panic` | 2026-07-14 | Blocked: missing rootFS mounting (`VFS: Unable to mount root fs`) |
| `PixelExperience_surya-13.0` | `surya` | `13.0` | `generic_arm64` | `Kernel Panic` | 2026-07-14 | Blocked: init killed early (`Attempted to kill init!`) |

---

## Status Definitions

* **Untested**: ROM zip has not been run or installed.
* **Bootable Check Failed**: Blocked during `BootManager` verification tests (missing `boot.img`, incompatible header, etc.).
* **Kernel Panic**: QEMU starts, kernel boots, but hits a kernel panic before reaching userspace.
* **Userspace Active**: Kernel initializes successfully, `/init` starts executing, but halts before shell is available.
* **Boot Completed**: Complete system startup.
