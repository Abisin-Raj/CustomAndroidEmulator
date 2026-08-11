# Analysis Report: Phase 10.26 Report - Qualcomm Subsystem Power Manager (SPM v2.1) Hardware Contract Extraction
**Execution Status**: SUCCESS

## Executive Summary
Successfully extracted the exact hardware contract for Qualcomm Subsystem Power Manager (qcom,spm-v2.1 @ 0x17900000). Mapped MMIO region (0x17900000, size 0x1000), SAW2 configuration attributes, and SPM status register structures into configs/pixel_experience_profile.yaml.

## Environment details
- **Target Device**: Qualcomm Subsystem Power Manager (qcom,spm-v2.1)
- **MMIO Base**: 0x17900000 (Size: 0x00001000)
- **SAW Name**: saw
- **Profile Updated**: configs/pixel_experience_profile.yaml

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
--- Qualcomm SPM Register & Capability Semantics Matrix ---
| Offset | Register Name | Access | Default Value | Constraint | Description |
|---|---|---|---|---|---|
| `0x0000` | `SPM_SECURE` | `READ` | `0x00000000` | `SPM security status flag` | Security register checked during SPM initialization. |
| `0x0004` | `SPM_STS0` | `READ` | `0x00000001` | `SPM status register 0 (bit 0 = idle/ready state)` | Status register polled during SPM power state transitions. |
| `0x0008` | `SPM_CTL` | `READ_WRITE` | `0x00000000` | `SPM power mode control mask` | Control register written to configure CPU low power modes. |
| `0x000C` | `SAW2_CFG` | `READ_WRITE` | `0x00000001` | `SAW2 configuration register` | SAW2 configuration settings. |

--- Kernel Execution Context ---
```
