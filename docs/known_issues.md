# Active Engineering Backlog: Known Issues

This document tracks active blocker issues, kernel panic signatures, and debugging progress as we work on Android compatibility.

---

## Compatibility Status Backlog

| Blocker Issue | Status | First Observed | Target ROM | Notes / Diagnosed Cause |
| --- | --- | --- | --- | --- |
| `VFS: Unable to mount root fs` | **Open (Active Blocker)** | Log #001 | `PixelExperience_violet` | Kernel is booted raw without virtio-blk configurations, block device maps, or custom system overlays parsing. |
| `init: Attempted to kill init!` | **Open** | Log #002 | `PixelExperience_surya` | `/init` binary exited early or crashed (SELinux violation or missing slot suffix commandline indicator). |
| `Device Tree Missing` | *Not observed yet* | - | - | Android kernels expect DTB overlay parameter blocks mapped into physical memory. |
| `SELinux Violations` | *Not observed yet* | - | - | Core Android services fail start-up checks if SELinux is running in enforcing mode on generic virtualization hosts. |

---

## Blocker Deep-Dives

### Blocker #1: `VFS: Unable to mount root fs`
* **Panic Signature**:
  `Kernel panic - not syncing: VFS: Unable to mount root fs on unknown-block(0,0)`
* **Diagnosed Reason**:
  The Linux kernel was launched using QEMU parameter `-kernel` and `-initrd`, but the root filesystem (`system.new.dat.br`) is compressed inside a sparse block package and is not exposed as a block device.
* **Possible Resolutions**:
  1. Translate the sparse block partition package into raw `.img` formats in the background.
  2. Map the filesystem device as a virtual VirtIO block device (`-drive file=system.img,if=virtio`) inside `QemuArguments`.
  3. Ensure proper `root=/dev/ram0` or `root=/dev/virtioblk` flags are compiled into the dynamic kernel commandline.
