# Analysis Report: Phase 8.7 Qualcomm GCC Register Access Matrix & Spec
**Execution Status**: SUCCESS

## Executive Summary
Analyzed probe and clock initialization code at Entry [3] (0xFFFFFF80080DD330). Mapped all memory/MMIO register access instructions, offset patterns, and access types (READ/WRITE). Defined the register specification matrix for the minimal QEMU GCC hardware device model.

## Environment details
- **Target Base**: 0x00100000 (Qualcomm GCC physical base)
- **Region Size**: 0x001F0000 bytes
- **Subsystem**: Qualcomm SM6150 / Violet Global Clock Controller

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
--- Register Access Matrix ---
| Address | Instruction | Offset / Field | Access Type | Functional Semantics |
| :--- | :--- | :--- | :--- | :--- |
| `0xffffff80080dd334` | `str      x30, [x18], #8` | `8` | **WRITE** | General Control Register |
| `0xffffff80080dd338` | `stp      x29, x30, [sp, #0x40]` | `0x40` | **WRITE** | Stack Frame / Saved Context Pointer |
| `0xffffff80080dd344` | `ldr      x8, [x8, #0xe80]` | `0xe80` | **READ** | General Control Register |
| `0xffffff80080dd348` | `str      x27, [sp, #0x50]` | `0x50` | **WRITE** | Stack Frame / Saved Context Pointer |
| `0xffffff80080dd34c` | `stp      x26, x25, [sp, #0x60]` | `0x60` | **WRITE** | Stack Frame / Saved Context Pointer |
| `0xffffff80080dd350` | `stp      x24, x23, [sp, #0x70]` | `0x70` | **WRITE** | Stack Frame / Saved Context Pointer |
| `0xffffff80080dd358` | `stp      x22, x21, [sp, #0x80]` | `0x80` | **WRITE** | Stack Frame / Saved Context Pointer |
| `0xffffff80080dd360` | `stp      x20, x19, [sp, #0x90]` | `0x90` | **WRITE** | General Control Register |
| `0xffffff80080dd370` | `ldr      x24, [x0, #0x48]` | `0x48` | **READ** | Clock Descriptor Linked-List Pointer |
| `0xffffff80080dd384` | `ldr      x8, [x25, #0x38]!` | `0x38` | **READ** | Clock Descriptor Linked-List Pointer |
| `0xffffff80080dd390` | `ldr      x26, [x25]` | `0x0` | **READ** | General Control Register |
| `0xffffff80080dd3a8` | `ldr      x26, [x26]` | `0x0` | **READ** | General Control Register |
| `0xffffff80080dd3c0` | `ldr      x8, [x8]` | `0x0` | **READ** | General Control Register |
| `0xffffff80080dd3ec` | `ldr      x8, [x23, #0x28]` | `0x28` | **READ** | General Control Register |
| `0xffffff80080dd3f0` | `ldr      x9, [x8, #0x40]` | `0x40` | **READ** | Stack Frame / Saved Context Pointer |
| `0xffffff80080dd3f8` | `ldr      x8, [x8, #0x28]` | `0x28` | **READ** | General Control Register |
| `0xffffff80080dd404` | `ldrb     w8, [x8, #0x78]` | `0x78` | **READ** | General Control Register |
| `0xffffff80080dd420` | `stp      w22, w21, [x0, #8]` | `8` | **WRITE** | General Control Register |
| `0xffffff80080dd428` | `str      w20, [x0, #0x10]` | `0x10` | **WRITE** | Subsystem Control / Status Descriptor |
| `0xffffff80080dd42c` | `str      x19, [x0, #0x18]` | `0x18` | **WRITE** | General Control Register |
--- Disassembly Stream ---
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
0xffffff80080dd360: stp        x20, x19, [sp, #0x90]
0xffffff80080dd364: mov        x19, x4
0xffffff80080dd368: stur       x8, [x29, #-8]
0xffffff80080dd36c: mov        w20, w3
0xffffff80080dd370: ldr        x24, [x0, #0x48]
0xffffff80080dd374: mov        w22, w1
0xffffff80080dd378: tbnz       w24, #8, #0xffffff80080dd40c
0xffffff80080dd37c: tbz        w24, #7, #0xffffff80080dd3e4
0xffffff80080dd380: mov        x25, x23
0xffffff80080dd384: ldr        x8, [x25, #0x38]!
0xffffff80080dd388: cmp        x25, x8
0xffffff80080dd38c: b.eq       #0xffffff80080dd3e4
0xffffff80080dd390: ldr        x26, [x25]
0xffffff80080dd394: cmp        x25, x26
0xffffff80080dd398: b.eq       #0xffffff80080dd3e4
0xffffff80080dd39c: adrp       x27, #0xffffff8008080000
0xffffff80080dd3a0: add        x27, x27, #0x8b4
0xffffff80080dd3a4: b          #0xffffff80080dd3b4
0xffffff80080dd3a8: ldr        x26, [x26]
0xffffff80080dd3ac: cmp        x25, x26
0xffffff80080dd3b0: b.eq       #0xffffff80080dd3e4
0xffffff80080dd3b4: ldurb      w8, [x26, #-8]
0xffffff80080dd3b8: cbnz       w8, #0xffffff80080dd3a8
0xffffff80080dd3bc: ldur       x8, [x26, #-0x30]
0xffffff80080dd3c0: ldr        x8, [x8]
0xffffff80080dd3c4: sub        x9, x8, x27
0xffffff80080dd3c8: ror        x9, x9, #2
0xffffff80080dd3cc: cmp        x9, #7
```
