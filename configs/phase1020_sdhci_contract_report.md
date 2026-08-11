# Analysis Report: Phase 10.20 Report - Qualcomm SDHCI Mobile Storage Controller Hardware Contract Extraction
**Execution Status**: SUCCESS

## Executive Summary
Successfully extracted the exact hardware contract for Qualcomm SDHCI Mobile Storage Controller (qcom,sdhci-msm-v5 @ 0x007C4000). Mapped MMIO regions (hc_mem, cmdq_mem, cmdq_ice), SPI 641 IRQ specification, 8-bit bus width, nonremovable status, and register access structures into configs/pixel_experience_profile.yaml.

## Environment details
- **Target Device**: Qualcomm SDHCI Mobile Storage Controller (qcom,sdhci-msm-v5)
- **Host Controller Base**: 0x007C4000 (Size: 0x00001000)
- **CMDQ Base**: 0x007C5000 (Size: 0x00001000)
- **ICE Base**: 0x007C8000 (Size: 0x00008000)
- **IRQ Line**: SPI 641 (0x00 0x281 0x00)
- **Bus Width**: 8-bit eMMC bus width
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
--- Qualcomm SDHCI Register & Capability Semantics Matrix ---
| Offset | Register Name | Access | Default Value | Constraint | Description |
|---|---|---|---|---|---|
| `0x0040` | `SDHCI_CAPABILITIES` | `READ` | `0x376CD8B0` | `64-bit v3.00 SDHCI host capabilities bitmask` | SDHCI capabilities register read during sdhci_msm_probe. |
| `0x0024` | `SDHCI_PRESENT_STATE` | `READ` | `0x01F70000` | `Card inserted & write-protect notch clear` | Host controller state register indicating card insertion. |
| `0x010C` | `SDHCI_MSM_VENDOR_SPEC` | `READ_WRITE` | `0x00000000` | `Qualcomm MSM vendor specific control flags` | Qualcomm proprietary vendor capabilities & mode control. |
| `0x0114` | `SDHCI_MSM_MCI_CLK_CTL` | `READ_WRITE` | `0x00000000` | `MCI clock enable & divider control` | Qualcomm MCI clock control register. |

--- Kernel Execution Context ---
```
