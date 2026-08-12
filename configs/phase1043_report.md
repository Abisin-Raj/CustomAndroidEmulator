# Phase 10.43 Detailed Brief: LBA Sector Access Tracing & GPT Discovery Analysis

## Executive Summary
Phase 10.43 added granular LBA sector access tracing to `hw/arm/qcom_ufshc.c` for **LBA 0 (MBR/Boot Header)** and **LBA 1 (GPT Header)** to record the kernel's exact partition table scan patterns and verify sector data delivery from the backing ROM image (`PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img`).

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
UFS Host Controller           ⚡ IN PROGRESS Base 0x01D84000 | LBA Sector Reads & Partition Scanning
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
                                 └── 11. GPT Partition Device Registration   ← NEXT BLOCKER
```

---

## 2. Granular Code Modifications & Rationale (`hw/arm/qcom_ufshc.c`)

- **LBA 0 / LBA 1 Tracing Additions**:
  - In `READ (10)` (`0x28`), added byte inspection for `LBA == 0` (MBR / Boot Header) and `LBA == 1` (GPT Header signature `"EFI PART"`).
  - Logs the exact 16-byte hex header returned via DMA to verify that valid partition signatures reach guest physical memory without memory corruption or offset shifts.

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
STEP-13     | QEMU      | DMA Transfer Image LBA 0 Bytes   | 41-4E-44-52-4F-49-44-21 ("ANDROID!") | IMPLEMENTED
```

---

## 4. Strict Evidence Classification Matrix

| Dimension | Observation / Finding | Classification |
|---|---|---|
| **What the kernel did before** | Initialized host controller, completed INQUIRY, cleared interrupts, and executed UIC commands | **OBSERVED** |
| **What QEMU returned** | Real image bytes from `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img` for requested LBAs | **OBSERVED / IMPL** |
| **What the kernel did afterward** | Kernel ISR acknowledged `UTRCS` interrupt and cleared `UFSHCI_IS` | **OBSERVED** |
| **Why block device registration is pending** | Linux kernel storage subsystem processes partition tables to register `/dev/block/sda*` devices | **DERIVED** |

---

## 5. Git Commits & Artifacts
- **`CustomAndroidEmulator` Repo Commit**: `91d760a`
- **`qemu_source` Repo Commit**: `8606e89`
- **Artifact**: `configs/phase1043_report.md`

---

## 6. Technical Roadmap to Next Milestone (Phase 10.44)

In Phase 10.44, we will track the Linux SCSI block layer (`sd_probe`) as it parses partition tables across LBA 0-33, verifying the creation of partition block devices (`/dev/block/sda1` through `sdaN`), mounting of `/system` and `/vendor`, and execution of Android `/init`.
