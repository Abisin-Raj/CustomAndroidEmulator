# Analysis Report: Phase 10.14 Report - Qualcomm TLMM Pinctrl Hardware Contract Extraction
**Execution Status**: SUCCESS

## Executive Summary
Successfully extracted the exact hardware contract for Qualcomm TLMM Pinctrl / GPIO Controller (qcom,sm6150-pinctrl @ 0x03000000). Mapped MMIO regions, SPI 208 IRQ specification, GPIO cell bindings, and per-pin register access structures into configs/pixel_experience_profile.yaml.

## Environment details
- **Target Device**: Qualcomm TLMM Pinctrl Controller (qcom,sm6150-pinctrl)
- **MMIO Base**: 0x03000000 (Size: 0x00DC2000)
- **SPI Config Base**: 0x17C000F0 (Size: 0x00000050)
- **IRQ Line**: SPI 208 (0x00 0xd0 0x00)
- **GPIO Cells**: #gpio-cells = <2>
- **Interrupt Cells**: #interrupt-cells = <2>
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
--- Qualcomm TLMM Register & Pin Semantics Matrix ---
| Offset / Pattern | Register Name | Access | Default Value | Constraint | Description |
|---|---|---|---|---|---|
| `0x0000 + (1000 * pin)` | `TLMM_GPIO_CFG_N` | `READ_WRITE` | `0x00000000` | `Function mux, drive strength, pull-up/pull-down configuration` | Per-pin configuration register specifying multiplexing function and bias. |
| `0x0004 + (1000 * pin)` | `TLMM_GPIO_IN_OUT_N` | `READ_WRITE` | `0x00000000` | `bit 0 = IN state, bit 1 = OUT state` | GPIO data input / output register reflecting pin level. |
| `0x0008 + (1000 * pin)` | `TLMM_GPIO_INTR_CFG_N` | `READ_WRITE` | `0x00000000` | `Interrupt target CPU & trigger mode flags` | Interrupt routing & trigger type control register. |
| `0x000C + (1000 * pin)` | `TLMM_GPIO_INTR_STATUS_N` | `READ_WRITE` | `0x00000000` | `bit 0 = Pending IRQ flag` | Interrupt status bit reflecting pending GPIO IRQ. |

--- Kernel Execution Context ---
```
