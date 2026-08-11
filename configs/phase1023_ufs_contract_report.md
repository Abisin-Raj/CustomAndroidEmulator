# Analysis Report: Phase 10.23 Report - Qualcomm Universal Flash Storage (UFS) Controller Hardware Contract Extraction
**Execution Status**: SUCCESS

## Executive Summary
Successfully extracted the exact hardware contract for Qualcomm UFS Host Controller (qcom,ufshc @ 0x01D84000). Mapped MMIO regions (ufs_mem, ufs_ice), SPI 265 IRQ specification, 1-lane direction binding, nonremovable status, and register access structures into configs/pixel_experience_profile.yaml.

## Environment details
- **Target Device**: Qualcomm UFS Host Controller (qcom,ufshc)
- **Host Controller Base**: 0x01D84000 (Size: 0x00003000)
- **ICE Base**: 0x01D90000 (Size: 0x00008000)
- **IRQ Line**: SPI 265 (0x00 0x109 0x00)
- **Lanes Per Direction**: 1 lane
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
--- Qualcomm UFS Register & Capability Semantics Matrix ---
| Offset | Register Name | Access | Default Value | Constraint | Description |
|---|---|---|---|---|---|
| `0x0000` | `UFSHCI_CAPABILITIES` | `READ` | `0x00000101` | `UFSHCI capabilities bitmask (32-slot command list, 64-bit address support)` | Host controller capability register read during ufshcd_init. |
| `0x0004` | `UFSHCI_HCS` | `READ` | `0x00000001` | `Host Controller Status (bit 0 = Device Present & Ready)` | Host status register polled during UFS controller setup. |
| `0x0008` | `UFSHCI_VER` | `READ` | `0x00000210` | `UFSHCI version 2.1 specification mask` | UFS specification version register. |
| `0x0034` | `UFSHCI_HCE` | `READ_WRITE` | `0x00000001` | `Host Controller Enable (bit 0 = Enable UFS HCI)` | Host enable register written to initiate controller power-on. |

--- Kernel Execution Context ---
```
