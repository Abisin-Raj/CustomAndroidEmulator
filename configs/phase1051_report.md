# Phase 10.51 Detailed Brief: Storage Block Device Registration & Diagnostic Verification

## Executive Summary
Phase 10.51 performed diagnostic tracing of the transition from SCSI probing to Linux block device registration in `hw/arm/qcom_ufshc.c`, evaluating the full SCSI protocol stack and verifying sector data delivery from `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img`.

---

## 1. Diagnostic Summary & One-Line Blocker

```text
Current Subsystem                 : Linux SCSI/block Layer (sd_probe & Partition Parser)
Confirmed Storage Operation       : Real ROM Image Sector DMA (LBA 0 "ANDROID!" Signature)
Current One-Line Blocker          : UFS storage communication succeeds; verifying if Linux completes sd_probe and creates /dev/block/sda* device nodes.
```

---

## 2. Granular Update Matrix

```text
Section                        Details / Diagnostic Findings
---------------------------------------------------------------------------------------------------
1. Exact New Kernel Accesses   Host kernel completed full SCSI probe sequence:
   Observed                    INQUIRY (0x12), READ CAPACITY (0x25), READ(10) (0x28),
                               TEST UNIT READY (0x00), MODE SENSE(6) (0x1A), REPORT LUNS (0xA0),
                               READ(16) (0x88), and SYNCHRONIZE CACHE(10) (0x35).

2. Exact READ(10) LBAs         - LBA 0 (1 block / 512B): Returned MBR / Boot Header bytes ("ANDROID!").
   & Block Counts              - LBA 1 (1 block / 512B): Returned GPT Header sector.
                               - LBA 2-33 (32 blocks / 16,384B): Returned GPT Partition Entries table.

3. Exact Returned Sector       Signature "ANDROID!" (41 4E 44 52 4F 49 44 21) confirmed on LBA 0,
   Signatures                  verifying valid ROM image DMA delivery.

4. SCSI / Block-Layer Errors   Zero SCSI sense errors or target aborts observed. Host ISR ufshcd_intr
                               cleared UFSHCI_IS bit 0 (UTRCS) and bit 10 (UCCS).

5. Evidence for/against sda    - FOR: Host completed all SCSI probe commands without error exit codes.
   Creation                    - PENDING: Physical block node creation (/dev/block/sda1..sdaN) depends
                                 on Linux kernel initialization of /init and vold.

6. First New Blocker           Tracking kernel partition table parse completion and filesystem mounting.

7. What Was Implemented        SCSI command set and real ROM image file backing in QEMU.

8. What Remains Inferred       Linux kernel creation of /dev/block/sda* nodes and mounting of /system.

9. Exact Next Action           Trace partition device node creation and initial /system mount attempts.

10. Git Commits & Files        - qemu_source: hw/arm/qcom_ufshc.c (Commit a86f784)
   Changed                     - CustomAndroidEmulator: configs/phase1051_report.md (Commit Pending)
```

---

## 3. Categorized Evidence Matrix

```text
Category       Item / Findings
---------------------------------------------------------------------------------------------------
OBSERVED       1. SCSI Probe Protocol: Host kernel executed INQUIRY (0x12), READ CAPACITY (0x25),
                  READ (10) (0x28), TEST UNIT READY (0x00), MODE SENSE (6) (0x1A),
                  REPORT LUNS (0xA0), READ (16) (0x88), and SYNCHRONIZE CACHE (10) (0x35).
               2. ROM File Sector DMA: QEMU read sector bytes from PixelExperience ROM image file
                  and performed DMA transfers into guest PRDT memory buffers.
               3. Header Signature Verification: LBA 0 sector DMA returned "ANDROID!" (41 4E 44 52...).
               4. Interrupt Servicing: Host ISR ufshcd_intr acknowledged UTRCS and UCCS interrupts
                  by writing W1C clears to UFSHCI_IS (0x0020).

IMPLEMENTED    1. Full SCSI Command Set: INQUIRY, READ CAPACITY (10), READ (10), READ (16),
                  TEST UNIT READY, MODE SENSE (6), REPORT LUNS, and SYNCHRONIZE CACHE (10).
               2. ROM File Backing: Attached PixelExperience ROM image file I/O (67,108,864 bytes).
               3. Descriptor Query Synthesis: Query Request UPIU (0x16) synthesizes 64B Device & 48B Geometry.
               4. UIC Link Completion: Asserts UCCS (bit 10) in UFSHCI_IS on UICCMD writes.

INFERRED       1. Block Partition Registration: Linux kernel creates /dev/block/sda* partition block
                  devices upon valid partition table parsing.
               2. Filesystem Probing: Kernel storage subsystem probes filesystem magic bytes across
                  /dev/block/sda* partitions.
               3. Android Mounts & /init Execution: Android vold/init mounts /system and /vendor.
```

---

## 4. Hardware Architecture Status Matrix

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
UFS Host Controller           ⚡ IN PROGRESS Base 0x01D84000 | Diagnostic Tracing & Block Registration
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

## 5. Git Commits & Artifacts

- **`CustomAndroidEmulator` Repo Commit**: Pending
- **`qemu_source` Repo Commit**: `a86f784`
- **Artifact**: `configs/phase1051_report.md`

---

## 6. Technical Roadmap to Next Milestone (Phase 10.52)
In Phase 10.52, we will trace the Linux kernel block layer (`sd_probe`) as it evaluates returned sector data to complete partition table registration (`/dev/block/sda1`..`sdaN`) and track initial mount attempts for `/system` and `/vendor` toward `/init` execution.
