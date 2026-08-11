# Analysis Report: Phase 10.11 Report - RPMh RSC Hardware Contract Extraction
**Execution Status**: SUCCESS

## Executive Summary
Successfully extracted the exact hardware contract for Qualcomm RPMh Resource State Coordinator (RSC / TCS @ 0x18220000) and RPMh Clock Controller (qcom,rpmh-clk-sm6150). Mapped MMIO regions, TCS channel bindings, IRQ specifications, and required driver behavioral contracts for QEMU implementation.

## Environment details
- **Target Machine**: Qualcomm SM6150 (PixelExperience Kernel)
- **RSC Base**: 0x18220000 (DRV2 Base: 0x18220D00)
- **RSC Compatible**: qcom,tcs-drv
- **RPMh Clock Compatible**: qcom,rpmh-clk-sm6150
- **RSC Interrupt**: SPI 5 (0x00 0x05 0x00)
- **DRV ID**: 0x02 (Apps RSC)

## Register Dump
```text
=== CPU 0 Register Snapshot ===
  X0 =0x0000000000000001  X1 =0x0000000000000000  X2 =0x000000000000000c  X3 =0x000000002e2e7364
  X4 =0x000000000000000c  X5 =0xffffff800a93579c  X6 =0xffffff800a957bcc  X7 =0x0000000000000000
  X8 =0xffffff8008080238  X9 =0x0000000000000002  X10=0x0000000000000000  X11=0xffffffffffffffff
  X12=0xffffff800a957bb2  X13=0x0000000000000000  X14=0x0000000000000001  X15=0xffffff800a957000
  X16=0x00000000000000e4  X17=0x0000000000000000  X18=0xffffff800a604048  X19=0xffffff800a8a3180
  X20=0x0000000000000001  X21=0xffffff800a931000  X22=0xffffff8008080230  X23=0x0000000000000000
  X24=0xffffff800a6f1000  X25=0x00000000000003e8  X26=0x0000000000000000  X27=0x000000004cccf320
  X28=0xffffff800a8a2000  X29=0xffffff800a6039c0  X30=0xffffff80080dc1d8
  SP =0xffffff800a6038b0   PC =0xffffff80080dc230   LR =0xffffff80080dc1d8
  PSTATE=0x00000000  CurrentEL=0
--- System EL1 Control Registers ---
  SCTLR =0x0000000000000000   TTBR0=0x0000000000000000
  TTBR1 =0x0000000000000000   TCR  =0x0000000000000000
  MAIR  =0x0000000000000000   ESR  =0x0000000000000000   FAR=0x0000000000000000
```

## Disassembly Context
```text
--- RPMh RSC Hardware Behavioral Specification Matrix ---
| Subsystem | Compatible | MMIO Base/Size | IRQ | TCS Configuration | Required Behavior |
|---|---|---|---|---|---|
| Qualcomm RPMh RSC Mailbox Controller | `qcom,tcs-drv` | `Base: 0x18220000 (Size: 0x100), TCS: 0x18220D00 (Size: 0x3000)` | `SPI 5 (0x00 0x05 0x00)` | `qcom,drv-id = <0x02> (DRV2 Apps RSC), 8 TCS channels` | Provides Trigger Control Sequence (TCS) channel slots and RSC interrupt status registers. |
| Qualcomm RPMh Clock Controller | `qcom,rpmh-clk-sm6150` | `Bound via Mailbox phandle (mboxes = <&apps_rsc 0x00>)` | `N/A (Uses RSC Mailbox)` | `Clock cells = <0x01>` | Exposes RPMh clock domain trees (cx_ao, mx_ao, rf_clk) backed by RSC TCS commands. |

--- Kernel Execution Context ---
0xFFFFFF80080DD330: sub      sp, sp, #0xa0
0xFFFFFF80080DD334: str      x30, [x18], #8
0xFFFFFF80080DD338: stp      x29, x30, [sp, #0x40]
0xFFFFFF80080DD33C: add      x29, sp, #0x40
0xFFFFFF80080DD340: adrp     x8, #0xffffff800a654000
0xFFFFFF80080DD344: ldr      x8, [x8, #0xe80]
0xFFFFFF80080DD348: str      x27, [sp, #0x50]
0xFFFFFF80080DD34C: stp      x26, x25, [sp, #0x60]
```
