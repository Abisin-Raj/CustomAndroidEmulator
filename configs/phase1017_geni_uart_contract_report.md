# Analysis Report: Phase 10.17 Report - Qualcomm QUPv3 / GENI UART Hardware Contract Extraction
**Execution Status**: SUCCESS

## Executive Summary
Successfully extracted the exact hardware contract for Qualcomm QUPv3 Serial Engine (qcom,qupv3-geni-se @ 0x008C0000) and GENI Console UART (qcom,msm-geni-console @ 0x00880000). Mapped MMIO regions, SPI 601 IRQ specification, wrapper core bindings, and register access structures into configs/pixel_experience_profile.yaml.

## Environment details
- **Target QUPv3 Wrapper**: Qualcomm QUPv3 Serial Engine (qcom,qupv3-geni-se @ 0x008C0000)
- **Target GENI UART**: Qualcomm GENI Console UART (qcom,msm-geni-console @ 0x00880000)
- **UART MMIO Base**: 0x00880000 (Size: 0x00004000)
- **QUPv3 MMIO Base**: 0x008C0000 (Size: 0x00006000)
- **IRQ Line**: SPI 601 (0x00 0x259 0x00)
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
--- Qualcomm GENI UART Register & Sequencer Semantics Matrix ---
| Offset | Register Name | Access | Default Value | Constraint | Description |
|---|---|---|---|---|---|
| `0x0040` | `GENI_STATUS` | `READ` | `0x00000000` | `bit 0 == 0 (M_GENI_CMD_ACTIVE idle bitmask)` | GENI Serial Engine status register polled during command setup. |
| `0x0068` | `GENI_FW_REVISION_VERSION` | `READ` | `0x00000100` | `GENI FW protocol revision magic` | Hardware firmware revision read during qcom_geni_serial_probe. |
| `0x0600` | `GENI_SER_M_CMD0` | `WRITE` | `0x00000000` | `Main sequencer opcode (0x08 = START_TX, 0x01 = INIT)` | Main sequencer command issue register. |
| `0x0610` | `GENI_SER_M_IRQ_STATUS` | `READ_WRITE` | `0x00000001` | `bit 0 == 1 (M_CMD_DONE_EN)` | Main sequencer IRQ status register for TX/RX completion. |

--- Kernel Execution Context ---
```
