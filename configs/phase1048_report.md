# Phase 10.48 Detailed Brief: Linux SCSI Disk Registration & Block Probe Evaluation

## Executive Summary
Phase 10.48 evaluated the Linux SCSI disk driver (`sd_probe`) registration boundary in `hw/arm/qcom_ufshc.c`, expanding SCSI command handling to include **SCSI `REPORT LUNS` (`0xA0`)** and **`READ (16)` (`0x88`)** to satisfy 64-bit block storage probe routines and trace partition table registration.

---

## 1. Subsystem Focus & Boundary Metrics

```text
Current Subsystem                 : Linux SCSI/block Layer (sd_probe & Partition Parser)
Current Blocker                   : Verifying /dev/block/sda* Node Registration & Probe State
Last Confirmed Storage Operation   : Real ROM Image Sector DMA (LBA 0 "ANDROID!" Signature)
Next Target                       : Trace First Post-Probe Failure or Partition File Node Mount
```

---

## 2. Categorized Evidence Matrix

### A. OBSERVED (Directly Verified Runtime Traces)
1. **SCSI Probe Commands Executed**:
   - Host kernel issued **SCSI INQUIRY (`0x12`)**, **READ CAPACITY (`0x25`)**, **READ (10) (`0x28`)**, **TEST UNIT READY (`0x00`)**, and **MODE SENSE (6) (`0x1A`)**.
   - Host driver submitted **SCSI REPORT LUNS (`0xA0`)** to query target logical unit layout.
2. **Sector DMA Delivery**:
   - QEMU reads bytes from file offset $\text{LBA} \times 512$ in `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img` and performs physical DMA writes into guest PRDT memory buffers.
   - LBA 0 sector DMA returns `"ANDROID!"` (`41 4E 44 52 4F 49 44 21`), matching Android boot image / partition sector data.
3. **Interrupt Servicing**:
   - Host ISR `ufshcd_intr` acknowledges `UTRCS` (bit 0) and `UCCS` (bit 10) interrupts by writing `W1C` clears to `UFSHCI_IS` (`0x0020`).

### B. IMPLEMENTED (Hardware Emulation Added in QEMU)
1. **SCSI `REPORT LUNS` (`0xA0`)**: Synthesizes 16-byte payload returning 8-byte LUN list length (`0x00000008`) and LUN 0 address (`0x0000000000000000`).
2. **SCSI `READ (16)` (`0x88`)**: Extracted 64-bit LBA and 32-bit transfer length to perform 64-bit sector reads from `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img`.
3. **SCSI `TEST UNIT READY` (`0x00`)**: Returns GOOD status (`0x00`) and Response UPIU `0x37`.
4. **SCSI `MODE SENSE (6)` (`0x1A`)**: Returns 4-byte Mode Parameter Header (`0x03, 0x00, 0x00, 0x00` - Write-back cache enabled, no write protection).

### C. INFERRED (Derived Architectural Next Steps)
1. **Partition Device Node Creation**: Linux kernel creates `/dev/block/sda*` partition block devices upon valid partition table parsing.
2. **Filesystem Probing**: Kernel storage subsystem probes filesystem magic bytes (`ext4`, `erofs`, `f2fs`) across `/dev/block/sda*` partitions.
3. **Android Mounts & `/init` Execution**: Android `vold`/`init` mounts `/system` and `/vendor` partitions toward launching `/init`.

---

## 3. Hardware Architecture Status Matrix

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
UFS Host Controller           ⚡ IN PROGRESS Base 0x01D84000 | SCSI REPORT LUNS & READ (16)
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
                                 ├── 11. SCSI TEST UNIT READY & MODE SENSE  ✓ OBSERVED
                                 ├── 12. SCSI REPORT LUNS (0xA0) & READ (16)✓ IMPLEMENTED
                                 └── 13. Linux Block Partition Node Registration ← NEXT BLOCKER
```

---

## 4. Git Commits & Modified Files

- **`qemu_source` Repository**:
  - File: `hw/arm/qcom_ufshc.c`
  - Commit: `a86f784` ("Phase 10.47: Implement SCSI TEST UNIT READY (0x00) and MODE SENSE (6) (0x1A) handlers")
- **`CustomAndroidEmulator` Repository**:
  - File: `configs/phase1048_report.md`
  - Commit: Pending

---

## 5. Technical Roadmap to Next Milestone (Phase 10.49)

In Phase 10.49, we will trace the Linux kernel block layer (`sd_probe`) as it evaluates returned sector data to complete partition table registration (`/dev/block/sda1`..`sdaN`) and track initial mount attempts for `/system` and `/vendor` toward `/init` execution.
