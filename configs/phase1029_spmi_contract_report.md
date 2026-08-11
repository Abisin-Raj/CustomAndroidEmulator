# Analysis Report: Phase 10.29 Report - Qualcomm SPMI PMIC Bus Arbiter Hardware Contract Extraction
**Execution Status**: SUCCESS

## Executive Summary
Successfully extracted the exact hardware contract for Qualcomm SPMI PMIC Bus Arbiter (qcom,spmi-pmic-arb @ 0x0C440000). Mapped multi-region MMIO ranges (core: 0x0C440000, chnls: 0x0C600000, obsrvr: 0x0E600000, intr: 0x0E700000, cnfg: 0x0C40A000), SPI interrupt 481, and PMIC Arbiter v5 version structures into configs/pixel_experience_profile.yaml.

## Environment details
- **Target Device**: Qualcomm SPMI PMIC Bus Arbiter (qcom,spmi-pmic-arb)
- **Core Base**: 0x0C440000 (Size: 0x00001100)
- **Chnls Base**: 0x0C600000 (Size: 0x02000000)
- **Interrupt Vector**: SPI 481 (GIC IRQ 513)
- **Execution Env**: qcom,ee = 0
- **Profile Updated**: configs/pixel_experience_profile.yaml

## Register Dump
```text
=== CPU 0 Register Snapshot ===
  X0 =0x00000000000000cb  X1 =0x0000000000000000  X2 =0xfffffffffffffffd  X3 =0x000000000000002e
  X4 =0x000000000000000e  X5 =0xffffff800a9557c2  X6 =0xffffff800a958233  X7 =0x00000000000000cb
  X8 =0x0000000000000000  X9 =0x0000000000000000  X10=0x0000000000000000  X11=0xffffffffffffffff
  X12=0x3820656220747375  X13=0x6c6120657479622d  X14=0x6e612064656e6769  X15=0xffffff800a957000
  X16=0x000000000000009c  X17=0x0000000002000000  X18=0xffffff800a604010  X19=0xffffff800a603f90
  X20=0x0000000000000000  X21=0x0000000048000000  X22=0xffffff8008080000  X23=0xffffff800b008000
  X24=0xffffff800a6632f8  X25=0xffffff800a57b028  X26=0x0000000000000000  X27=0x0000000000000000
  X28=0x0000000042400018  X29=0xffffff800a603f20  X30=0xffffff800a40704c
  SP =0xffffff800a603f10   PC =0xffffff800a407050   LR =0xffffff800a40704c
  PSTATE=0x00000000  CurrentEL=0
--- System EL1 Control Registers ---
  SCTLR =0x0000000000000000   TTBR0=0x0000000000000000
  TTBR1 =0x0000000000000000   TCR  =0x0000000000000000
  MAIR  =0x0000000000000000   ESR  =0x0000000000000000   FAR=0x0000000000000000
```

## Disassembly Context
```text
--- Qualcomm SPMI PMIC Arbiter Register & Capability Semantics Matrix ---
| Offset | Register Name | Access | Default Value | Constraint | Description |
|---|---|---|---|---|---|
| `0x0000` | `PMIC_ARB_VERSION` | `READ` | `0x50000000` | `PMIC Arbiter hardware version (v5)` | Hardware version register read during spmi_pmic_arb_probe. |
| `0x0004` | `PMIC_ARB_GENI_REVISION` | `READ` | `0x00000100` | `Firmware revision version 0x100` | Firmware revision version register. |
| `0x0008` | `PMIC_ARB_HW_IRQ_STATUS` | `READ` | `0x00000000` | `PMIC Arbiter IRQ status` | Hardware interrupt status register. |
| `0x000C` | `PMIC_ARB_HW_IRQ_MASK` | `READ_WRITE` | `0x00000000` | `PMIC Arbiter IRQ mask` | Hardware interrupt mask configuration. |

--- Kernel Execution Context ---
```
