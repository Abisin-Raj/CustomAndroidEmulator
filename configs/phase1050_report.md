# Phase 10.50 Detailed Brief: Post-SCSI Probe Diagnostic & Execution State Evaluation

## Executive Summary
Phase 10.50 conducted a diagnostic evaluation of the post-probe execution state in `hw/arm/qcom_ufshc.c` to trace the kernel's exact transition following SCSI disk initialization and identify the precise boundary before block partition registration (`/dev/block/sda*`).

---

## 1. Subsystem Focus & Diagnostic Boundary

```text
Current Subsystem                 : Linux SCSI/block Layer (sd_probe & Block Registration)
Current Blocker                   : Proving /dev/block/sda* Node Registration & Partition Scan Completion
Last Confirmed Storage Operation   : Real ROM Image Sector DMA (LBA 0 "ANDROID!" Signature)
Next Target                       : Trace First File Node Mount Attempt or Storage Subsystem Stop
```

---

## 2. Categorized Evidence Matrix

### A. OBSERVED (Directly Verified Runtime Traces)
1. **Full SCSI Protocol Handshake**:
   - Host kernel executed **INQUIRY (`0x12`)**, **READ CAPACITY (`0x25`)**, **READ (10) (`0x28`)**, **TEST UNIT READY (`0x00`)**, **MODE SENSE (6) (`0x1A`)**, **REPORT LUNS (`0xA0`)**, **READ (16) (`0x88`)**, and **SYNCHRONIZE CACHE (10) (`0x35`)**.
2. **Sector DMA Delivery**:
   - QEMU reads bytes from file offset $\text{LBA} \times 512$ in `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img` and performs physical DMA writes into guest PRDT memory buffers.
   - LBA 0 sector DMA returns `"ANDROID!"` (`41 4E 44 52 4F 49 44 21`), matching Android boot image / partition sector data.
3. **Interrupt Servicing**:
   - Host ISR `ufshcd_intr` acknowledges `UTRCS` (bit 0) and `UCCS` (bit 10) interrupts by writing `W1C` clears to `UFSHCI_IS` (`0x0020`).

### B. IMPLEMENTED (Hardware Emulation Added in QEMU)
1. **Full SCSI Command Set**: `INQUIRY`, `READ CAPACITY (10)`, `READ (10)`, `READ (16)`, `TEST UNIT READY`, `MODE SENSE (6)`, `REPORT LUNS`, and `SYNCHRONIZE CACHE (10)`.
2. **ROM File Backing**: Attached `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img` (67,108,864 bytes / 131,072 sectors) to serve sector reads via DMA.
3. **Descriptor Query Synthesis**: `Query Request UPIU (0x16)` synthesizes 64B Device Descriptor and 48B Geometry Descriptor.
4. **UIC Link Completion**: Asserts `UCCS` (bit 10) in `UFSHCI_IS` on `UICCMD` writes.

### C. INFERRED (Derived Architectural Next Steps)
1. **Block Partition Registration**: Linux kernel creates `/dev/block/sda*` partition block devices upon valid partition table parsing.
2. **Filesystem Probing**: Kernel storage subsystem probes filesystem magic bytes across `/dev/block/sda*` partitions.
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
UFS Host Controller           ⚡ IN PROGRESS Base 0x01D84000 | Post-Probe Diagnostic & Block Registration
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
                                 ├── 12. SCSI REPORT LUNS (0xA0) & READ (16)✓ OBSERVED
                                 ├── 13. SCSI SYNCHRONIZE CACHE (10) (0x35) ✓ OBSERVED
                                 └── 14. Linux Block Partition Node Creation ← NEXT BLOCKER
```

---

## 4. Git Commits & Artifacts

- **`CustomAndroidEmulator` Repo Commit**: `c0006f4`
- **`qemu_source` Repo Commit**: `a86f784`
- **Artifact**: `configs/phase1050_report.md`

---

## 5. Technical Roadmap to Next Milestone (Phase 10.51)

In Phase 10.51, we will trace the Linux kernel block layer (`sd_probe`) as it evaluates returned sector data to complete partition table registration (`/dev/block/sda1`..`sdaN`) and track initial mount attempts for `/system` and `/vendor` toward `/init` execution.
