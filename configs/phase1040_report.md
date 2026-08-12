# Phase 10.40 Detailed Brief: Query Request UPIU & UFS Descriptor Handling

## Executive Summary
Phase 10.40 added Query Request UPIU (`Type 0x16`) handling for UFS **Device Descriptors** (IDN `0x00`, 64 bytes) and **Geometry Descriptors** (IDN `0x07`, 48 bytes) in `hw/arm/qcom_ufshc.c`, completing the initialization requirements for UFS link and device configuration query routines.

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
UFS Host Controller           ⚡ IN PROGRESS Base 0x01D84000 | UTP DMA / SCSI INQUIRY / Descriptors
                                 │
                                 ├── 1. Controller Init & Capability Read   ✓ OBSERVED
                                 ├── 2. UTRDLR Base Setup (0x0050/0x0054)   ✓ OBSERVED
                                 ├── 3. Slot 0 UTRDBR Ring (0x0058 = 1)      ✓ OBSERVED
                                 ├── 4. SCSI INQUIRY (0x12) DMA Intercept    ✓ OBSERVED
                                 ├── 5. Response UPIU (0x37) & 36B Payload  ✓ OBSERVED
                                 ├── 6. UTRCS Interrupt & Host W1C Clear    ✓ OBSERVED
                                 ├── 7. UFSHCI_IE (0x0030) Enable Read     ✓ OBSERVED
                                 ├── 8. UICCMD (0x0090) Link Command Intercept ✓ OBSERVED
                                 ├── 9. Query Request (0x16) Descriptors     ✓ IMPLEMENTED (0x00/0x07)
                                 └── 10. Block Storage Read / Partition Table ← NEXT BLOCKER
```

---

## 2. Granular Code Modifications & Rationale (`hw/arm/qcom_ufshc.c`)

- **Descriptor Parameter Parsing**:
  - Intercepts UPIU `trans_type == 0x16` (Query Request).
  - Reads `QueryRequest` parameters at `trd.cmd_desc_base_addr + 12`: `opcode`, `idn`, `index`, `selector`, `length`.
- **Synthesized Query Response UPIU (`Type 0x36`)**:
  - `response = 0x00` (Target Success), `status = 0x00` (GOOD).
  - Sets `data_segment_len` to returned descriptor size (64 bytes for Device, 48 bytes for Geometry).
- **Device Descriptor (IDN `0x00`)**:
  - Constructs 64-byte UFS 2.1 Device Descriptor (`wSpecVersion = 0x0210`, `bNumberLU = 8`, `bInitPowerMode = 1`).
- **Geometry Descriptor (IDN `0x07`)**:
  - Constructs 48-byte Geometry Descriptor (`qTotalRawDeviceCapacity = 64 GB`, `bMaxNumberLU = 8`).
- **PRDT Data Buffer Transfer**:
  - DMA transfers synthesized descriptor payload to host buffer address `prd.base_addr`.
- **Completion Signaling**:
  - Sets `trd.overall_cmd_status = 0`, clears slot in `s->utrdbr`, asserts `s->is |= 0x1` (`UTRCS`), and raises GIC IRQ 243.

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
STEP-10     | Kernel    | Write 0x01D84058 (UTRDBR)        | Query Request 0x16 | DERIVED / IMPL
STEP-11     | QEMU      | Synthesize Query Resp (0x36)     | Device/Geometry Descriptor | IMPLEMENTED
```

---

## 4. Strict Evidence Classification Matrix

| Dimension | Observation / Finding | Classification |
|---|---|---|
| **What the kernel did before** | Executed host init, SCSI INQUIRY, ISR interrupt clear, and UIC link command (`0x0090`) | **OBSERVED** |
| **What QEMU returned** | INQUIRY response, UCCS interrupt, and Device/Geometry Descriptor Query Responses (`0x36`) | **OBSERVED / IMPL** |
| **What the kernel did afterward** | Kernel driver executed ISR handlers without host controller panics | **OBSERVED** |
| **Why block device is pending** | Kernel driver requires SCSI `READ CAPACITY` `0x25` & sector backing to expose `/dev/block/sd*` | **DERIVED** |

---

## 5. Git Commits & Artifacts
- **`CustomAndroidEmulator` Repo Commit**: `e939035`
- **`qemu_source` Repo Commit**: `8606e89`
- **Artifact**: `configs/phase1040_report.md`

---

## 6. Technical Roadmap to Next Milestone (Phase 10.41)

In Phase 10.41, we will handle SCSI **`READ CAPACITY (10)` (`0x25`)** and **`READ (10)` (`0x28`)** requests in `hw/arm/qcom_ufshc.c`, mapping block data from `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img` so the kernel reads GPT headers and registers Android block devices (`/dev/block/sda*`).
