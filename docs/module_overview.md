# Module Overview

This document provides a directory-by-directory overview of the CustomAndroidEmulator codebase.

---

## 1. Directory Structure

```text
CustomAndroidEmulator/
├── launcher/          # Qt6 GUI executable (Main Dashboard, ROMs list, Settings, Timeline checkboxes)
├── core/              # Logger wrapper (spdlog) and ConfigManager (settings.json, vm_profiles.json)
├── models/            # Shared data types, states, severities, event definitions, metadata
├── rom_manager/       # ROM installation zip extractions, uninstallations, directory cleanups
├── image_parser/      # Binary parsers (boot.img headers v0-v4, vbmeta, ext4 magics checks)
├── boot_manager/      # ROM verification checklist checks (avb, kernel sizes, slots)
├── boot_assets/       # In-memory kernel/ramdisk offset carvers and temporary assets manager
├── boot_events/       # EventDispatcher and signal emission coordinators
├── boot_session/      # BootSession statistic logs and boot_<ROM>_<timestamp>.log files generator
├── qemu_backend/      # QemuDetector PATH locator, arguments builders, background process launcher
├── boot_pipeline/     # IBootDetector class registry (Kernel, Init, Panic log line matching)
├── virtual_machine/   # VmManager coordinates starting VM execution and logs forwarding
└── tests/             # Programmatic test suite (52 check assertions)
```

---

## 2. Directory Summaries

### `launcher`
* **Type**: Executable
* **Responsibilities**: Displays widgets to browse/install ROM zips, set custom QEMU paths, displays real-time checkboxes checklist boot progression, timelines severity lists, and crash registers/backtraces analyzer edit boxes.

### `core`
* **Type**: Static Library
* **Responsibilities**: Exposes logging macros. Persists user variables inside `settings.json` and loads dynamic virtual board parameters templates from `vm_profiles.json`.

### `image_parser`
* **Type**: Static Library
* **Responsibilities**: Safely handles file seeking and binary decoding on partition blocks to parse header attributes (sizes, offsets, OS versions, release strings).

### `boot_pipeline`
* **Type**: Static Library
* **Responsibilities**: Implements the base `IBootDetector` class. Hosts priority-sorted detectors (`PanicDetector` = 150, `KernelDetector` = 100, `InitDetector` = 50) that process logs as they arrive and advance the state machine.
