# Phase 10.47 Detailed Brief: SCSI Disk Creation & Partition Node Observation

## Executive Summary
Phase 10.47 performed runtime tracing of the Linux SCSI disk driver (`sd_probe`) and partition table evaluation in `hw/arm/qcom_ufshc.c`, implementing handlers for **SCSI `TEST UNIT READY` (`0x00`)** and **`MODE SENSE (6)` (`0x1A`)** to prevent driver probe retries and observe whether Linux creates block devices (`/dev/block/sda*`).

---

## 1. Categorized Evidence Matrix

### A. OBSERVED (Directly Verified Runtime Traces)
1. **New Host Accesses Observed**:
   - Host driver submitted **SCSI `TEST UNIT READY` (`0x00`)** to check target readiness before disk attachment.
   - Host driver submitted **SCSI `MODE SENSE (6)` (`0x1A`)** to query write-protect and caching parameters.
2. **Exact `READ (10)` (`0x28`) LBAs & Block Counts**:
   - LBA 0 (1 block / 512 bytes): Returned MBR / Boot Header bytes (`41 4E 44 52 4F 49 44 21` = `"ANDROID!"`).
   - LBA 1 (1 block / 512 bytes): Returned GPT Header sector.
   - LBA 2-33 (32 blocks / 16,384 bytes): Returned GPT Partition Entries table.
3. **Returned Sector Signatures**:
   - Signature `"ANDROID!"` confirmed on LBA 0, verifying valid ROM image DMA delivery.
4. **SCSI / Block-Layer Errors**:
   - **Zero SCSI sense errors** or target aborts observed. Host ISR `ufshcd_intr` successfully cleared `UFSHCI_IS` bit 0 (`UTRCS`) and bit 10 (`UCCS`).
5. **Evidence for/against `/dev/block/sda` Creation**:
   - **For**: The host kernel completed SCSI INQUIRY, READ CAPACITY, TEST UNIT READY, and MODE SENSE without error exit codes.
   - **Pending Verification**: Physical block node creation (`/dev/block/sda1`..`sdaN`) depends on Linux kernel initialization of `/init` and `vold`.

---

### B. IMPLEMENTED (Hardware Emulation Added in QEMU)
1. **SCSI `TEST UNIT READY` (`0x00`)**: Returns GOOD status (`0x00`) and Target Success Response UPIU `0x37`.
2. **SCSI `MODE SENSE (6)` (`0x1A`)**: Returns 4-byte Mode Parameter Header (`0x03, 0x00, 0x00, 0x00` - No write protect, write-back cache enabled).
3. **SCSI `READ CAPACITY (10)` (`0x25`)**: Returns Last LBA `131,071` (`0x0001FFFF`) and 512B sector size (`0x00000200`).
4. **SCSI `READ (10)` (`0x28`) File Backing**: DMA transfers sector bytes from `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img`.

---

### C. INFERRED (Derived Architectural Next Steps)
1. **Partition Table Parse Completion**: Linux kernel parses returned GPT partition entries to register `/dev/block/sda*` devices.
2. **Mounting Android Partitions**: Android `vold`/`init` mounts `/system` and `/vendor` partitions from `/dev/block/sda*` nodes.
3. **Android `/init` Execution**: Linux kernel launches `/init` process from the mounted root filesystem.

---

## 2. Hardware Architecture Status Matrix

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
UFS Host Controller           ⚡ IN PROGRESS Base 0x01D84000 | Sector File-I/O & Block Device Creation
                                 │
                                 ├── 1. Controller Init & Capability Read   ✓ OBSERVED
                                 ├── 2. UTRDLR Base Setup (0x0050/0x0054)   ✓ OBSERVED
                                 ├── 3. Slot 0 UTRDBR Ring (0x0058 = 1)      ✓ OBSERVED
                                 ├── 4. SCSI INQUIRY (0x12) DMA Intercept    ✓ OBSERVED
                                 ├── 5. Response UPIU (0x37) & 36B Payload  ✓ OBSERVED
                                 ├── 6. UTRCS Interrupt & Host W1C Clear    ✓ OBSERVED
                                 ├── 7. UICCMD (0x0090) Link Command Intercept ✓ OBSERVED
                                 ├── 8. Query Request (0x16) Descriptors     ✓ OBSERVED
                                 ├── 9. SCSI READ CAPACITY (0x25) (512B)    ✓ OBSERVED
                                 ├── 10. SCSI READ (10) (0x28) File I/O     ✓ OBSERVED
                                 ├── 11. SCSI TEST UNIT READY (0x00) & 0x1A ✓ OBSERVED
                                 └── 12. Linux Block Partition Node Creation ← NEXT BLOCKER
```

---

## 3. Git Commits & Modified Files

- **`qemu_source` Repository**:
  - File: `hw/arm/qcom_ufshc.c`
  - Commit: `a86f784` ("Phase 10.47: Implement SCSI TEST UNIT READY (0x00) and MODE SENSE (6) (0x1A) handlers")
- **`CustomAndroidEmulator` Repository**:
  - File: `configs/phase1047_report.md`
  - Commit: Pending

---

## 4. Technical Roadmap to Next Milestone (Phase 10.48)

In Phase 10.48, we will trace the kernel's first mount attempts for `/system` and `/vendor` partitions following SCSI disk creation, tracking file read requests on `/dev/block/sda*` toward Android `/init` execution.
