# Analysis Report: Phase 8.5 Real Entry [3] Registration & Probe Path Tracing
**Execution Status**: SUCCESS

## Executive Summary
Verified that QEMU's default virt machine device tree (virt.dtb) contains standard 'arm,armv8-timer' nodes but lacks Qualcomm vendor clock controller nodes ('qcom,gcc-violet'). Because vendor DT nodes are absent from virt.dtb, the Qualcomm clock driver probe is never triggered during initcalls, leaving [X21+0xBE0] at fallback entry [2].

## Environment details
- **Target Machine**: QEMU virt (aarch64)
- **Ops Pointer**: 0xffffff8008080238 (Fallback Entry [2])
- **Real Entry [3] Target**: 0xFFFFFF80080DD330

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
--- Device Tree Inspection Summary ---
- **virt.dtb**: No Qualcomm clock/timer nodes
- **qcom_generated.dtb**: File not present in workspace
- **qcom_clusters.dtb**: File not present in workspace
--- Real Entry [3] Code Snippet ---
0xffffff80080dd330: sub        sp, sp, #0xa0
0xffffff80080dd334: str        x30, [x18], #8
0xffffff80080dd338: stp        x29, x30, [sp, #0x40]
0xffffff80080dd33c: add        x29, sp, #0x40
0xffffff80080dd340: adrp       x8, #0xffffff800a654000
0xffffff80080dd344: ldr        x8, [x8, #0xe80]
0xffffff80080dd348: str        x27, [sp, #0x50]
0xffffff80080dd34c: stp        x26, x25, [sp, #0x60]
0xffffff80080dd350: stp        x24, x23, [sp, #0x70]
0xffffff80080dd354: mov        x23, x0
0xffffff80080dd358: stp        x22, x21, [sp, #0x80]
0xffffff80080dd35c: mov        w21, w2
```
