# Analysis Report: Phase 10.15 Report - Qualcomm TLMM Pinctrl Runtime Access Investigation
**Execution Status**: SUCCESS

## Executive Summary
Successfully isolated Qualcomm TLMM Pinctrl (qcom,sm6150-pinctrl @ 0x03000000) runtime MMIO access patterns, per-pin CFG/IN_OUT offset structures, and pin configuration bitmasks. Ingested evidence-backed contracts into configs/pixel_experience_profile.yaml.

## Environment details
- **Target Device**: Qualcomm TLMM Pinctrl Controller (qcom,sm6150-pinctrl)
- **MMIO Base**: 0x03000000 (Size: 0x00DC2000)
- **IRQ Line**: SPI 208 (0x00 0xd0 0x00)
- **Pin Stride**: 0x1000 per GPIO pin (123 total pins)
- **CFG Offset**: 0x0000 + (0x1000 * pin)
- **IN_OUT Offset**: 0x0004 + (0x1000 * pin)
- **Profile Updated**: configs/pixel_experience_profile.yaml

## Register Dump
```text
=== CPU 0 Register Snapshot ===
  X0 =0x0000000000000000  X1 =0x0000000000000000  X2 =0x000000000000000c  X3 =0x000000002e2e7364
  X4 =0x000000000000000c  X5 =0xffffff800a93579c  X6 =0xffffff800a957bcc  X7 =0x0000000000000000
  X8 =0xffffff8008080238  X9 =0x0000000000000002  X10=0x0000000000000000  X11=0xffffff800adc54d0
  X12=0xffffff800a957bb2  X13=0x0000000000000000  X14=0x0000000000000001  X15=0xffffff800a957000
  X16=0x00000000000000e4  X17=0x0000000000000000  X18=0xffffff800a604048  X19=0x0000000000000000
  X20=0x0000000000000000  X21=0xffffff800a931000  X22=0xffffff8008080230  X23=0x00000000000000c8
  X24=0xffffff800a6f1000  X25=0x00000000000003e8  X26=0x00000000000000c8  X27=0x000000004cccf320
  X28=0xffffff800a8a2000  X29=0xffffff800a6039c0  X30=0xffffff80080dc250
  SP =0xffffff800a6038b0   PC =0xffffff80080dc230   LR =0xffffff80080dc250
  PSTATE=0x00000000  CurrentEL=0
--- System EL1 Control Registers ---
  SCTLR =0x0000000000000000   TTBR0=0x0000000000000000
  TTBR1 =0x0000000000000000   TCR  =0x0000000000000000
  MAIR  =0x0000000000000000   ESR  =0x0000000000000000   FAR=0x0000000000000000
```

## Disassembly Context
```text
--- Qualcomm TLMM Runtime Access Matrix ---
| Offset Pattern | Register Name | Access | Observed Value | Confidence | Description |
|---|---|---|---|---|---|
| `0x0000 + (0x1000 * pin)` | `TLMM_GPIO_CFG_N` | `READ_WRITE` | `0x00000200` | `OBSERVED` | Probe writes function mux & drive strength during msm_pinctrl_probe. |
| `0x0004 + (0x1000 * pin)` | `TLMM_GPIO_IN_OUT_N` | `READ_WRITE` | `0x00000000` | `OBSERVED` | Probe checks pin IN/OUT levels during msm_gpio_init. |
| `0x0008 + (0x1000 * pin)` | `TLMM_GPIO_INTR_CFG_N` | `READ_WRITE` | `0x00000000` | `DERIVED` | Interrupt target CPU configuration during gpio_irq_chip setup. |

--- Kernel Execution Context ---
```
