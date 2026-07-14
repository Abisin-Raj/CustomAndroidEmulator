# Boot Process Pipeline

This document explains the real-time execution flow when a user clicks **Launch** on an installed ROM inside the CustomAndroidEmulator.

---

## 1. Execution Flow

### Step 1: Pre-launch checks & configurations
* `VmManager::startVm` initiates.
* `QemuDetector` queries the system environment and settings to retrieve `qemu-system-aarch64` binary route.
* `VmConfigurationBuilder` reads the ROM's `boot_config.json` parameters.

### Step 2: Carving Boot Assets
* `BootAssetLoader` seek-carves the kernel and ramdisk segment ranges from `boot.img` directly in memory.
* It creates a safe cache folder `_tmp_boot/` inside the ROM's path.
* It writes the buffers to `_tmp_boot/kernel` and `_tmp_boot/ramdisk.img`.

### Step 3: Preparing Arguments
* `VmProfile` loads the board profile properties from `vm_profiles.json` (e.g. virt, cortex-a53, 1024M).
* `KernelCommandLine` reformats the kernel cmdline parameters from the ROM header, injecting console redirect args:
  `console=ttyAMA0 earlycon=pl011,0x09000000 printk.devkmsg=on`
* `QemuArguments` assembles the final argument string list.

### Step 4: Spawning Process
* `QemuLauncher` executes QEMU as a background process.
* Starts an elapsed session timer inside `VmManager`.
* State transitions to `VM_STARTING` (checklist: `✓ VM Started`).

### Step 5: Real-time Event Logging
* As QEMU streams serial guest lines to stdout/stderr:
  * `QemuLauncher` emits `logLineRead` signal.
  * `VmManager` forwards the string and elapsed seconds offset to `BootPipelineManager`.
  * The pipeline scans the line across registered detectors in order:
    1. **`PanicDetector`** (priority 150): Scans for backtraces, registers dumps, and triggers `EMULATION_FAILED` state.
    2. **`KernelDetector`** (priority 100): Scans for `Linux version`, PSCI controllers, and CPUs online. Updates state to `KERNEL_RUNNING` (checklist: `✓ Kernel Running`).
    3. **`InitDetector`** (priority 50): Scans for `/init` stages, ramdisk mount outputs, and system services starts. Updates state to `USERSPACE_STARTING` (checklist: `✓ Userspace Starting`) and then `SYSTEM_SERVICES` (checklist: `✓ System Services`).
  * Structured `BootEvent` messages (Info/Warning/Error/Critical) are dispatched to the GUI timeline logs lists.

### Step 6: Session Termination & Logs Saving
* When QEMU exits or the user clicks **Stop VM**:
  * `BootSession` compiles timelines statistics.
  * It exports a log file to `logs/boot_<ROM>_<timestamp>.log`.
  * `BootAssetLoader::cleanTempAssets` completely purges the `_tmp_boot/` temporary folder.
  * State indicators are unlocked, and launcher status returns to `Ready`.
