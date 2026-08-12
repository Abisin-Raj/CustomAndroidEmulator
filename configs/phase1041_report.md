# Phase 10.41 Detailed Brief: SCSI READ CAPACITY & Sector Read Handlers

## Executive Summary
Phase 10.41 implemented SCSI **`READ CAPACITY (10)` (Opcode `0x25`)** and **`READ (10)` (Opcode `0x28`)** command handlers in `hw/arm/qcom_ufshc.c`, advancing the UFS host controller model to return block device geometry (64 GB, 4096-byte sectors) and prepare image backing for GPT partition parsing.

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
UFS Host Controller           ⚡ IN PROGRESS Base 0x01D84000 | SCSI READ CAPACITY / Sector Reads
                                 │
                                 ├── 1. Controller Init & Capability Read   ✓ OBSERVED
                                 ├── 2. UTRDLR Base Setup (0x0050/0x0054)   ✓ OBSERVED
                                 ├── 3. Slot 0 UTRDBR Ring (0x0058 = 1)      ✓ OBSERVED
                                 ├── 4. SCSI INQUIRY (0x12) DMA Intercept    ✓ OBSERVED
                                 ├── 5. Response UPIU (0x37) & 36B Payload  ✓ OBSERVED
                                 ├── 6. UTRCS Interrupt & Host W1C Clear    ✓ OBSERVED
                                 ├── 7. UICCMD (0x0090) Link Command Intercept ✓ OBSERVED
                                 ├── 8. Query Request (0x16) Descriptors     ✓ OBSERVED
                                 ├── 9. SCSI READ CAPACITY (0x25)            ✓ IMPLEMENTED
                                 └── 10. SCSI READ (10) (0x28) Image Backing ← NEXT BLOCKER
```

---

## 2. Granular Code Modifications & Rationale (`hw/arm/qcom_ufshc.c`)

- **SCSI `READ CAPACITY (10)` (Opcode `0x25`) Handler**:
  - Intercepts CDB byte 0 `0x25`.
  - Synthesizes 8-byte payload: Last LBA `0x07FFFFFF` (16,777,215 logical blocks), Block Length `0x00001000` (4096 bytes per sector = 64 GB capacity).
  - Copies payload to `prd.base_addr` via physical DMA, sets `overall_cmd_status = 0`, clears slot in `s->utrdbr`, and asserts `UTRCS` interrupt.
- **SCSI `READ (10)` (Opcode `0x28`) Handler**:
  - Intercepts CDB byte 0 `0x28`.
  - Extracts 32-bit Logical Block Address (LBA) from CDB bytes `2-5` and 16-bit Transfer Length (block count) from CDB bytes `7-8`.
  - Synthesizes Response UPIU `0x37`, clears slot in `s->utrdbr`, and asserts `UTRCS` interrupt to complete DMA block transfers.

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
STEP-11     | QEMU      | Write 8B Read Capacity Payload   | Max LBA=0x07FFFFFF, Sector=4096 | IMPLEMENTED
STEP-12     | Kernel    | Write 0x01D84058 (UTRDBR)        | SCSI READ (10) 0x28 | DERIVED / IMPL
```

---

## 4. Strict Evidence Classification Matrix

| Dimension | Observation / Finding | Classification |
|---|---|---|
| **What the kernel did before** | Initialized host controller, issued INQUIRY `0x12`, cleared interrupts, and executed UIC link calls | **OBSERVED** |
| **What QEMU returned** | Synthesized INQUIRY response, Device/Geometry descriptors, and 8-byte `READ CAPACITY` data | **OBSERVED / IMPL** |
| **What the kernel did afterward** | Kernel ISR acknowledged `UTRCS` interrupt and cleared `UFSHCI_IS` | **OBSERVED** |
| **Why GPT parsing is pending** | Direct file-backed sector DMA reading from `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img` needs attachment to `0x28` handler | **DERIVED** |

---

## 5. Git Commits & Artifacts
- **`CustomAndroidEmulator` Repo Commit**: `97830fb`
- **`qemu_source` Repo Commit**: `8606e89`
- **Artifact**: `configs/phase1041_report.md`

---

## 6. Technical Roadmap to Next Milestone (Phase 10.42)

In Phase 10.42, we will attach file I/O reading from `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img` into the SCSI `READ (10)` (`0x28`) DMA handler in `hw/arm/qcom_ufshc.c`. When the kernel requests LBA 0 (MBR) and LBA 1 (GPT Header), QEMU will read the actual partition table bytes from disk and DMA-transfer them to `prd.base_addr`, causing the kernel to discover system, vendor, product, and userdata partitions.
