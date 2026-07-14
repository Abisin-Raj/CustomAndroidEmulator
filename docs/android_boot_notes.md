# Engineering Notebook: Android Boot Notes

This document acts as an engineering logbook to capture boot tests, panic logs, and investigations for guest custom ROMs.

---

## Observations & Case Logs

### Log #001: PixelExperience violet
* **ROM Name**: `PixelExperience_violet-13.0-20240127-0711-OFFICIAL`
* **Device**: Xiaomi Redmi Note 7 Pro (`violet`)
* **OS / Android**: `13.0`
* **Virtual Profile**: `violet` (Cortex-A76, `virt` board, 2048MB RAM)
* **Status reached**: `EMULATION_FAILED` (Kernel Panic)
* **Elapsed timing**: `2.4s`
* **Log Snip**:
  ```text
  [    2.400000] CPU: 0 PID: 1 Comm: swapper/0 Not tainted
  [    2.405000] Kernel panic - not syncing: VFS: Unable to mount root fs on unknown-block(0,0)
  ```
* **Suggested Cause**: Missing root filesystem mounting layer. The system partition resides inside `system.new.dat.br` (Brotli block compressed format) and cannot be mounted directly as a block device by QEMU.
* **Next Action**: Implement the dynamic block conversion layer in Phase 6 to expand the Brotli sparse system partition into raw ext4.

---

### Log #002: PixelExperience surya
* **ROM Name**: `PixelExperience_surya-13.0-20230818-1213-OFFICIAL`
* **Device**: Xiaomi POCO X3 (`surya`)
* **OS / Android**: `13.0`
* **Virtual Profile**: `generic_arm64` (Cortex-A53, `virt` board, 1024MB RAM)
* **Status reached**: `EMULATION_FAILED` (Kernel Panic)
* **Elapsed timing**: `1.8s`
* **Log Snip**:
  ```text
  [    1.800000] Kernel panic - not syncing: Attempted to kill init! exitcode=0x00000007
  ```
* **Suggested Cause**: AOSP `/init` binary exited early or failed execution checks (likely due to slot suffix expectations or SELinux policies setup).
* **Next Action**: Map slot variables in kernel commandline options inside Phase 6 and trace early mount instructions.

---

## Primary Target Reference ROM

To maintain focus, the following ROM package is selected as our primary target baseline for compatibility improvements:
* **Package**: `PixelExperience_violet-13.0-20240127-0711-OFFICIAL`
* **Platform version**: Android 13
* **Validation target**: Progress this baseline through the milestones list sequentially.

---

## Project Milestones

Instead of generic broad phases, we measure emulator progress by 5 concrete functional milestones:

1. **Milestone 1**: QEMU launches successfully, ARM64 kernel executes early setup code. (✓ **Achieved**)
2. **Milestone 2**: Kernel reaches `start_kernel()` and registers virtual timers/interrupts. (✓ **Achieved**)
3. **Milestone 3**: `/init` executes (userspace starts up). (✗ **Pending** - blocked by VFS mount)
4. **Milestone 4**: Core Android system services begin (HALs, service manager, binder). (✗ **Pending**)
5. **Milestone 5**: Android launcher GUI home screen rendering completes. (✗ **Pending**)
