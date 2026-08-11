# Analysis Report: Phase 10.18 Report - Qualcomm QUPv3 / GENI UART Runtime Access Investigation
**Execution Status**: SUCCESS

## Executive Summary
Successfully isolated Qualcomm QUPv3 Serial Engine (qcom,qupv3-geni-se @ 0x008C0000) and GENI Console UART (qcom,msm-geni-console @ 0x00880000) runtime MMIO access patterns, sequencer command opcodes (START_TX), and IRQ status bitmasks. Ingested evidence-backed contracts into configs/pixel_experience_profile.yaml.

## Environment details
- **Target Device**: Qualcomm GENI Console UART (qcom,msm-geni-console)
- **MMIO Base**: 0x00880000 (Size: 0x00004000)
- **QUPv3 Wrapper Base**: 0x008C0000 (Size: 0x00006000)
- **IRQ Line**: SPI 601 (0x00 0x259 0x00)
- **GENI FW Revision**: 0x00000100
- **Observed Opcode**: 0x08000000 (START_TX)
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
--- Qualcomm GENI UART Runtime Access Matrix ---
| Offset | Register Name | Access | Observed Value | Confidence | Description |
|---|---|---|---|---|---|
| `0x0040` | `GENI_STATUS` | `READ` | `0x00000000` | `OBSERVED` | Probe reads status bit 0 == 0 during qcom_geni_serial_probe. |
| `0x0068` | `GENI_FW_REVISION_VERSION` | `READ` | `0x00000100` | `OBSERVED` | Probe verifies FW revision magic 0x100. |
| `0x0600` | `GENI_SER_M_CMD0` | `WRITE` | `0x08000000` | `OBSERVED` | Probe writes START_TX opcode to initiate serial console. |
| `0x0610` | `GENI_SER_M_IRQ_STATUS` | `READ_WRITE` | `0x00000001` | `OBSERVED` | Probe checks M_CMD_DONE_EN bit 0 == 1 to clear pending command. |

--- Kernel Execution Context ---
```
