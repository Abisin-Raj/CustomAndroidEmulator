# Compatibility Hardware Profiles

This document details how hardware configurations are loaded from `vm_profiles.json` and mapped to virtual machines.

---

## 1. Profiles Structure

The `ConfigManager` reads `vm_profiles.json` relative to the application binary. Default profile templates are automatically written on first boot:

```json
[
    {
        "name": "generic_arm64",
        "cpu": "cortex-a53",
        "machine": "virt",
        "memory": "1024M",
        "nographic": true
    },
    {
        "name": "violet",
        "cpu": "cortex-a76",
        "machine": "virt",
        "memory": "2048M",
        "nographic": true
    }
]
```

---

## 2. Decoupling Profiles from Physical Hardware

> [!WARNING]
> **Do not overfit VM Profiles to physical device chips.**
> Real devices run complex system-on-chips (SoCs) such as Qualcomm Snapdragon (e.g. Snapdragon 675 for Xiaomi `violet`), which feature unique proprietary timers, interrupt controllers, and custom hardware registers.
> QEMU does not emulate these proprietary mobile SoCs directly. Instead, it exposes a generic virtualized `virt` board architecture.

### Mapping Principle
The virtual machine profile outlines the QEMU machine board setup (`virt`) and virtual CPU capabilities, **not** the physical phone components. 

The compatibility layer (Phase 6) resolves the bridge between phone-specific layouts and virtual layouts. For example, routing:
* Qualcomm UART ➔ QEMU PL011 UART
* Qualcomm GIC ➔ QEMU GICv3
* eMMC Partitioning ➔ VirtIO Block Storage
* Device Tree Blobs (DTB) ➔ QEMU Virtual Device Tree
