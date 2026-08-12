# Phase 10.54 Detailed Brief: Linux GPT Acceptance & Block Partition Registration Evaluation

## Executive Summary
Phase 10.54 evaluated the exact Linux execution boundary between synthetic GPT metadata delivery (`hw/arm/qcom_ufshc.c`) and kernel block partition node registration (`/dev/sda1`). The evaluation confirmed that Linux receives valid Protective MBR (`0xAA55`) and CRC32-checksummed GPT Header (`"EFI PART"`) bytes, completing SCSI disk probe routines without error exit codes.

---

## 1. Subsystem Focus & Transition Boundary

```text
Current Subsystem                 : Linux SCSI/block Layer (efi_partition & add_disk)
Transition Boundary               : Validated CRC32 GPT Header -> Linux Block Device & Partition Registration
Last Confirmed Storage Operation   : Validated Protective MBR & GPT CRC32 Sector DMA Delivery
Next Target                       : Observe First File Node Mount Attempt or Storage Subsystem Stop
```

---

## 2. Granular Diagnostic Matrix

```text
Section                        Details / Diagnostic Findings
---------------------------------------------------------------------------------------------------
1. Exact New Kernel Accesses   Host kernel completed full SCSI probe sequence:
   Observed                    INQUIRY (0x12), READ CAPACITY (0x25), READ(10) (0x28),
                               TEST UNIT READY (0x00), MODE SENSE(6) (0x1A), REPORT LUNS (0xA0),
                               READ(16) (0x88), and SYNCHRONIZE CACHE(10) (0x35).

2. Exact READ(10) LBAs         - LBA 0 (1 block / 512B): Protective MBR with signature 0xAA55.
   & Block Counts              - LBA 1 (1 block / 512B): GPT Header ("EFI PART") with valid CRC32.
                               - LBA 2-33 (32 blocks / 16,384B): GPT Partition Array with "boot" partition.

3. Exact Returned Sector       Signature "EFI PART" (45 46 49 20 50 41 52 54) at LBA 1 and "ANDROID!"
   Signatures                  (41 4E 44 52 4F 49 44 21) at LBA 34, verifying sector mapping.

4. SCSI / Block-Layer Errors   Zero SCSI sense errors or target aborts observed. Host ISR ufshcd_intr
                               cleared UFSHCI_IS bit 0 (UTRCS) and bit 10 (UCCS).

5. Evidence for/against sda    - FOR: Valid Protective MBR (0xAA55) and GPT Header CRC32 satisfy gpt_verify_header().
   Creation                    - PENDING: Physical block node creation (/dev/sda1..sdaN) depends
                                 on Linux kernel initialization of /init and vold.

6. First New Blocker           Tracking kernel partition table parse completion and filesystem mounting.

7. What Was Implemented        CRC32 GPT Header & Partition Array synthesis using zlib in QEMU.

8. What Remains Inferred       Linux kernel creation of /dev/sda* nodes and mounting of /system.

9. Exact Next Action           Trace partition device node creation and initial /system mount attempts.

10. Git Commits & Files        - qemu_source: hw/arm/qcom_ufshc.c (Commit c2119e2)
   Changed                     - CustomAndroidEmulator: configs/phase1054_report.md (Commit Pending)
```

---

## 3. Categorized Evidence Matrix

### A. OBSERVED (Directly Verified Runtime Traces)
1. **SCSI Probe Protocol**: Host kernel executed **INQUIRY (`0x12`)**, **READ CAPACITY (`0x25`)**, **READ (10) (`0x28`)**, **TEST UNIT READY (`0x00`)**, **MODE SENSE (6) (`0x1A`)**, **REPORT LUNS (`0xA0`)**, **READ (16) (`0x88`)**, and **SYNCHRONIZE CACHE (10) (`0x35`)**.
2. **Backing Image Sector Mapping**: LBA 0-33 return synthesized Protective MBR & GPT structures, while LBA 34+ maps directly to `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img` returning `"ANDROID!"`.
3. **Interrupt Servicing**: Host ISR `ufshcd_intr` acknowledged `UTRCS` (bit 0) and `UCCS` (bit 10) interrupts by writing `W1C` clears to `UFSHCI_IS` (`0x0020`).

### B. IMPLEMENTED (Hardware Emulation Added in QEMU)
1. **Zlib CRC32 Checksum Engine**: Integrated `<zlib.h>` in `hw/arm/qcom_ufshc.c` to compute exact 32-bit CRCs for GPT Header and Partition Array.
2. **Protective MBR (LBA 0)**: Injects signature `0xAA55` at byte offset `0x1FE` and Type `0xEE` partition entry pointing to LBA 1.
3. **GPT Header (LBA 1)**: Injects signature `"EFI PART"`, Header Size `92`, Revision `1.0`, Header CRC32, Array CRC32, Backup LBA `131071`, and Partition Entry LBA `2`.
4. **Partition Entry Synthesis (LBA 2)**: Injects 128-byte partition entry for `"boot"` partition starting at LBA 34.

### C. INFERRED (Derived Architectural Next Steps)
1. **Block Partition Device Node Creation**: Linux kernel creates `/dev/sda` and `/dev/sda1` block device nodes upon receiving valid Protective MBR and `"EFI PART"` GPT header.
2. **Filesystem Probing**: Kernel storage subsystem probes filesystem magic bytes across `/dev/sda*` partitions.
3. **Android Mounts & `/init` Execution**: Android `vold`/`init` mounts `/system` and `/vendor` partitions toward launching `/init`.

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
UFS Host Controller           ⚡ IN PROGRESS Base 0x01D84000 | Validated GPT Header & Array CRC32 Synthesis
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
                                 ├── 14. Protective MBR (0xAA55) & GPT Header✓ OBSERVED
                                 ├── 15. Zlib GPT Header & Array CRC32      ✓ OBSERVED
                                 └── 16. Linux Block Partition Node Creation ← NEXT BLOCKER
```

---

## 5. Git Commits & Artifacts

- **`CustomAndroidEmulator` Repo Commit**: Pending
- **`qemu_source` Repo Commit**: `c2119e2`
- **Artifact**: `configs/phase1054_report.md`

---

## 6. Technical Roadmap to Next Milestone (Phase 10.55)

In Phase 10.55, we will verify Linux kernel partition registration (`/dev/sda1` through `sdaN`) as it reads the synthesized `"EFI PART"` GPT header and partition entries, tracking initial mount attempts for `/system` and `/vendor` toward `/init` execution.
