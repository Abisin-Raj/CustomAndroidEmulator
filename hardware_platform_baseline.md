# Qualcomm Virtual Hardware Platform — Hardware Baseline (Phase 1.1)

**Date:** September 24, 2026  
**Target Device:** Xiaomi Redmi Note 7 Pro (`violet` / Qualcomm Snapdragon 675 / SM6150)  
**Host Architecture:** QEMU ARM64 (`qemu-system-aarch64`) with Custom Qualcomm Emulation  
**Status:** Phase 1.1 Baseline Established  

---

## 1. Executive Summary

This document establishes the verified hardware baseline for the Qualcomm virtual hardware platform running the pristine Redmi Note 7 Pro (SM6150) Android 13 operating system. 

The system currently boots an unmodified Qualcomm Android 13 vendor/system image through `init`, `servicemanager`, `hwservicemanager`, `zygote64`, `system_server`, `surfaceflinger`, and launches `bootanimation`. Checkpoint infrastructure (**CP-01** at SurfaceFlinger stability and **CP-02** at BootAnimation display token acquisition) allows instantaneous sub-second restoration to deterministic execution states without repeated cold boots.

---

## 2. Established Hardware Components (Working)

| Subsystem | Implementation Details | Hardware Contract / Parameters | Operational Status |
| :--- | :--- | :--- | :--- |
| **ARM64 CPU** | QEMU TCG Engine | Cortex-A55, ARMv8.2-A, single-core (`-smp 1 -cpu cortex-a55`), VHE, crypto extensions | **ACTIVE** — Executes full Qualcomm kernel & userspace instruction streams |
| **System Memory (RAM)** | QEMU RAM | 4096 MiB (`-m 4096`), Base PA: `0x40000000`, Kernel TTBR1: `0x43006000` | **ACTIVE** — Linear kernel mapping + user space virtual memory |
| **Interrupt Controller** | ARM GICv3 | `-M virt,gic-version=3`, GICD at `0x08000000`, GICR at `0x080a0000`, PPI/SPI lines | **ACTIVE** — Dispatches timers, I/O interrupts, and virtual device signals |
| **eMMC Host Controller** | `hw/sd/qcom-sdhci-msm.c` | MMIO base `0x007c4000` (`7c4000.sdhci`), IRQ 16 (GIC 48), Qualcomm MCI core, vendor capabilities, DLL control | **ACTIVE** — Partitions (`boot`, `system`, `vendor`, `product`, `data`) mounted read/write via `emmc_overlay.qcow2` |
| **SPMI PMIC Arbiter** | `hw/misc/qcom-spmi-pmic-arb.c` | MMIO base `0x0c400000` (`c400000.spmi`), SPMI Arbiter v5 layout, channel mapping, PMIC transactions | **ACTIVE** — Services Qualcomm PMIC drivers (`pm6150`, `pm6150l`), regulator queries |
| **Global Clock Controller (GCC)** | `hw/misc/qcom-gcc-sm6150.c` | MMIO base `0x01400000` (`1400000.clock-controller`), SM6150 RCG2 root clocks & branch gates | **ACTIVE** — Handles clock frequency configuration; auto-clears UPDATE and CLK_OFF bits to prevent driver polling timeouts |
| **Qualcomm SCM / SMC** | `target/arm/tcg/psci.c` | Intercepts Qualcomm Secure Channel Manager (SCM) SMC calls (`0x02000000` SIP/OEM series) | **ACTIVE** — Responds to TrustZone/SCM queries from kernel without trapping |
| **ION Memory Allocator** | Device Tree (`virt_spmi_compact_ion.dtb`) | Compact ION carveout reservations and contiguous memory pools | **ACTIVE** — Allocates gralloc and media buffers across kernel and HALs |
| **Android Boot Chain** | Guest Storage + Kernel | Pristine kernel (`kernel.img`), initrd (`boot_ramdisk.gz`), Android 13 (`PixelExperience violet`) | **ACTIVE** — Boots `init` -> `servicemanager` -> `zygote` -> `system_server` -> `surfaceflinger` |
| **DRM / HWC Software Path** | Vendor HAL + SurfaceFlinger | `android.hardware.graphics.composer@2.3-service` + SurfaceFlinger presentation loop | **ACTIVE** — Connects display tokens, registers services, and processes layer composition |
| **Display Output Engine** | QEMU `ramfb` | `-device ramfb`, resolution $1080 \times 2340$, 32-bpp BGRA linear framebuffer mapped to host SDL window | **ACTIVE** — Provides visible display surface |
| **Synthetic VSYNC** | SurfaceFlinger Software Source | Timer-driven synthetic software VSYNC generator (60 Hz cadence) | **ACTIVE** — Clocks SurfaceFlinger composition passes without physical vblank IRQs |
| **Graphics Runtime (ANGLE / SwiftShader)** | Android Userspace Framework | ANGLE (libEGL / libGLESv2) translating to Vulkan / Pastel CPU rasterizer | **ACTIVE** — Provides OpenGL ES / Vulkan software fallback |
| **Recovery Checkpoints** | QEMU QCOW2 Internal Snapshots | `CP-01` (`cp01_sf_stable`) and `CP-02` (`cp02_rtr_token`) | **ACTIVE** — 1.09s deterministic state restore via QMP `loadvm` |

---

## 3. Currently Synthetic, Bypassed, or Unmodeled Components

To achieve boot without hardware lockups, several Qualcomm proprietary subsystems are currently bypassed or operating through software fallbacks:

```text
========================================================================================
Subsystem                  Current Status           Impact / Bypass Mechanism
========================================================================================
Qualcomm GPU (Adreno 612)  NOT IMPLEMENTED          Guest falls back to software rendering (Pastel/SwiftShader).
                                                    No 3D hardware acceleration.

KGSL (/dev/kgsl-3d0)       NOT IMPLEMENTED          Kernel Graphics Support Layer driver is dormant/unbound.
                                                    Vulkan/GLES vendor drivers fail device enumeration.

MDSS / DPU Display Engine  NOT IMPLEMENTED          Qualcomm Mobile Display Subsystem (SDE/DPU) is absent.
                                                    Frame delivery uses QEMU ramfb instead of DRM/KMS scanout.

Display Hardware VSYNC     SYNTHETIC BYPASS         No hardware display vblank interrupts from DPU.
                                                    SurfaceFlinger relies on software timer VSYNC.

Qualcomm HWC Hardware Mode INTERCEPTED / SOFTWARE   HWC HAL operates in software fallback / null display path
                                                    rather than commanding physical MDSS pipes and overlays.

Qualcomm Modem / Baseband  UNMODELED                RIL / telephony daemons dormant or mocked.

Hexagon DSP (ADSP/CDSP)    UNMODELED                Compute and audio DSP interfaces unattached.

Camera Subsystem (Spectra) UNMODELED                Camera HALs dormant.
========================================================================================
```

---

## 4. Hardware Phase 1 Guiding Rules

1. **Evidence-Driven Implementation**: No virtual device is implemented based on assumptions. Implementation begins only when guest execution demonstrates a concrete, blocking dependency on that device.
2. **Software-Visible Contract**: We emulate the register/MMIO interface, IRQ behavior, and driver contract expected by the guest kernel and HALs, not internal physical silicon logic.
3. **No Destructive Guest Modifications**: The base disk (`emmc.img`) remains cryptographically frozen (`04ba06833ab9...`). All mutations and state testing occur on ephemeral snapshots on `emmc_overlay.qcow2`.
4. **Preserve Checkpoint Recovery**: Any new virtual device skeleton or machine modification must support full VMState migration/snapshotting so that `loadvm` / `savevm` remain fully functional.
