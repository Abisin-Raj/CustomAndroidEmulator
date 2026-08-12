# Phase 10.42 Detailed Brief: File-Backed Sector Reads & Backing ROM Image Integration

## Executive Summary
Phase 10.42 attached real backing storage file I/O reading from `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img` (67,108,864 bytes) to the SCSI **`READ (10)` (Opcode `0x28`)** handler in `hw/arm/qcom_ufshc.c`, matching geometry (512-byte sectors, 131,072 max LBAs) to allow the Linux kernel to perform true DMA sector transfers for partition discovery.

---

## 1. Hardware Architecture Status Matrix

```text
Component                     Status        Details / Register Contracts
---------------------------------------------------------------------------------------------------
GCC Clock Controller          ✓ RESOLVED    Base 0x00100000 | Clock gates & resets handled
ARMv8 Architectural Timer     ✓ RESOLVED    Generic Timer IRQs (PPI 13/14) active
PSCI / Secondary CPU          ✓ VERIFIED    PSCI v1.0 CPU_ON bringing up Cortex-A55 cores
RPMh RSC / Clock              ✓ RESOLVED    Base 0x17C00000 | RPMh command memory mapped
TLMM Pinctrl / GPIO           ✓ RESOLVED    Base 0x03000000 | GPIO pins & IRQs registered
QUPv3 / GENI UART             ✓ RESOLVED    Base 0x00880000 | TX FIFO (0x0700) stdout logging
SDHCI MSM Controller          ✓ RESOLVED    Base 0x007C4000 | Probe contract satisfied
SPM v2.1 Power Manager        ✓ RESOLVED    Base 0x17900000 | SAW2 / SPM control registers
SPMI PMIC Bus Arbiter         ✓ RESOLVED    Base 0x0C440000 | Arbiter v5 & PM6150 revid (0x00511B01)
UFS Host Controller           ⚡ IN PROGRESS Base 0x01D84000 | Sector File-I/O & DMA Transfers
                                 │
                                 ├── 1. Controller Init & Capability Read   ✓ OBSERVED
                                 ├── 2. UTRDLR Base Setup (0x0050/0x0054)   ✓ OBSERVED
                                 ├── 3. Slot 0 UTRDBR Ring (0x0058 = 1)      ✓ OBSERVED
                                 ├── 4. SCSI INQUIRY (0x12) DMA Intercept    ✓ OBSERVED
                                 ├── 5. Response UPIU (0x37) & 36B Payload  ✓ OBSERVED
                                 ├── 6. UTRCS Interrupt & Host W1C Clear    ✓ OBSERVED
                                 ├── 7. UICCMD (0x0090) Link Command Intercept ✓ OBSERVED
                                 ├── 8. Query Request (0x16) Descriptors     ✓ OBSERVED
                                 ├── 9. SCSI READ CAPACITY (0x25) (512B)    ✓ IMPLEMENTED
                                 ├── 10. SCSI READ (10) (0x28) File I/O     ✓ IMPLEMENTED
                                 └── 11. Kernel GPT / Partition Discovery   ← NEXT BLOCKER
```

---

## 2. Granular Code Modifications & Rationale (`hw/arm/qcom_ufshc.c`)

- **512-Byte Sector Size Geometry Alignment**:
  - Configured `READ CAPACITY (10)` (`0x25`) handler to report Last LBA `0x0001FFFF` (131,071 LBAs) and sector size `0x00000200` (512 bytes), precisely matching the 67,108,864-byte ROM image file size (`67,108,864 / 512 = 131,072` sectors).
- **Direct Image File I/O Sector Reading**:
  - In `READ (10)` (`0x28`), QEMU opens `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img`, calculates byte offset:
    $$\text{file\_offset} = \text{LBA} \times 512$$
  - Reads `blocks * 512` bytes into allocated buffer `img_buf`.
  - Performs physical DMA transfer (`cpu_physical_memory_write`) to host guest memory address `prd.base_addr`.
- **Completion Signaling**:
  - Writes Response UPIU `0x37`, sets `overall_cmd_status = 0`, clears slot in `s->utrdbr`, and asserts `UTRCS` interrupt.

---

## 3. Empirical Chronological Protocol Trace

```text
Sequence ID | Initiator | Hardware Action / Register Access | Payload / State | Classification
---------------------------------------------------------------------------------------------------
STEP-01     | Kernel    | Read 0x01D84000 (CAPABILITIES)   | 0x00000101      | OBSERVED
STEP-02     | Kernel    | Write 0x01D84050 / 0x01D84054    | 0x0000000040000000 | OBSERVED
STEP-03     | Kernel    | Write 0x01D84058 (UTRDBR)        | 0x00000001 (Slot 0) | OBSERVED
STEP-04     | QEMU      | DMA Read TRD & UPIU Header       | Type 0x21, SCSI INQUIRY 0x12 | OBSERVED
STEP-05     | QEMU      | DMA Write Resp UPIU & 36B Payload| Type 0x37, 36B SPC-4 | OBSERVED
STEP-06     | QEMU      | Assert IS Bit 0 (UTRCS) & IRQ    | IS |= 0x1, IRQ 243 | OBSERVED
STEP-07     | Kernel    | Write 0x01D84020 (UFSHCI_IS W1C) | 0x00000001 (Cleared)| OBSERVED
STEP-08     | Kernel    | Write 0x01D84090 (UFSHCI_UICCMD) | UIC Link Cmd      | OBSERVED
STEP-09     | QEMU      | Assert IS Bit 10 (UCCS) & IRQ    | IS |= 0x400, IRQ 243 | OBSERVED
STEP-10     | Kernel    | Write 0x01D84058 (UTRDBR)        | SCSI READ CAPACITY 0x25 | DERIVED / IMPL
STEP-11     | QEMU      | Write 8B Read Capacity Payload   | Max LBA=131071, Sector=512B | IMPLEMENTED
STEP-12     | Kernel    | Write 0x01D84058 (UTRDBR)        | SCSI READ (10) 0x28 (LBA 0)| DERIVED / IMPL
STEP-13     | QEMU      | File Read & DMA Sector Transfer  | Read file offset LBA*512 to PRDT | IMPLEMENTED
```

---

## 4. Strict Evidence Classification Matrix

| Dimension | Observation / Finding | Classification |
|---|---|---|
| **What the kernel did before** | Initialized controller, issued INQUIRY `0x12`, cleared interrupts, and executed UIC link calls | **OBSERVED** |
| **What QEMU returned** | Synthesized INQUIRY response, Device/Geometry descriptors, 512B `READ CAPACITY`, and real ROM bytes | **OBSERVED / IMPL** |
| **What the kernel did afterward** | Kernel ISR acknowledged `UTRCS` interrupt and cleared `UFSHCI_IS` | **OBSERVED** |
| **Why GPT parsing is pending** | Kernel driver requires full partition table scan across LBA 0 (MBR) and LBA 1-33 (GPT entries) | **DERIVED** |

---

## 5. Git Commits & Artifacts
- **`CustomAndroidEmulator` Repo Commit**: `8e98385`
- **`qemu_source` Repo Commit**: `8606e89`
- **Artifact**: `configs/phase1042_report.md`

---

## 6. Technical Roadmap to Next Milestone (Phase 10.43)

In Phase 10.43, we will trace the kernel's partition discovery routines as it reads LBA 0 (MBR) and LBA 1-33 (GPT Header & Partition Table Entries) via `READ (10)` (`0x28`), verifying that Linux registers `/dev/block/sda*` partitions (`system`, `vendor`, `product`, `boot`, `userdata`) and proceeds toward mounting the root filesystem and launching `/init`.
