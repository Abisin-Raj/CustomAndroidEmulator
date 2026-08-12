# Phase 10.52 Detailed Brief: Storage Block Device Registration & GPT Header Structure Diagnosis

## Executive Summary
Phase 10.52 conducted a diagnostic evaluation of the Linux SCSI disk driver (`sd_probe`) boundary and inspected the backing ROM image (`PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img`). Inspection confirmed that LBA 0 contains a raw Android boot image header (`"ANDROID!"`), while LBA 1 is zero-filled. QEMU was updated in `hw/arm/qcom_ufshc.c` to synthesize a valid **Protective MBR (LBA 0)** and **GPT Header (`"EFI PART"`, LBA 1)** so Linux partition scanning routines (`rescan_partitions`) can register block device nodes (`/dev/sda*`).

---

## 1. Subsystem Focus & Diagnostic Checklist

```text
Current Subsystem                 : Linux SCSI/block Layer (sd_probe & rescan_partitions)
Diagnostic Target                 : Verify transition from SCSI probe to block-device node creation
Key Finding                       : Backing file is raw Android boot image without LBA 1 GPT header; synthesized Protective MBR + 'EFI PART' GPT table.

Evidence Checklist Status:
[x] sd_probe entered              - OBSERVED (Host submits INQUIRY, READ CAPACITY, TEST UNIT READY, MODE SENSE)
[x] disk allocated                - OBSERVED (Host executes REPORT LUNS 0xA0 and allocates SCSI lun 0)
[x] capacity accepted             - OBSERVED (Host accepts 131,071 max LBA, 512B sector size)
[x] add_disk reached              - OBSERVED (Host executes SYNCHRONIZE CACHE 0x35)
[ ] add_disk completed            - PENDING (Kernel block layer registers /dev/sda)
[x] GPT scan entered              - OBSERVED (Host reads LBA 0, LBA 1, LBA 2-33)
[x] GPT accepted/rejected         - IMPLEMENTED (Synthesized Protective MBR at LBA 0 and 'EFI PART' at LBA 1)
[ ] partition count               - PENDING (Tracking partition table registration)
[ ] first partition registration  - PENDING (Tracking /dev/sda1 device node creation)
```

---

## 2. Categorized Evidence Matrix

### A. OBSERVED (Directly Verified Runtime Traces)
1. **SCSI Probe Protocol**: Host kernel executed **INQUIRY (`0x12`)**, **READ CAPACITY (`0x25`)**, **READ (10) (`0x28`)**, **TEST UNIT READY (`0x00`)**, **MODE SENSE (6) (`0x1A`)**, **REPORT LUNS (`0xA0`)**, **READ (16) (`0x88`)**, and **SYNCHRONIZE CACHE (10) (`0x35`)**.
2. **Backing Image Inspection**: Physical sector analysis of `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img` confirmed LBA 0 starts with `"ANDROID!"` (`41 4E 44 52 4F 49 44 21`), and LBA 1 (offset 0x200) contains zero bytes.
3. **Interrupt Servicing**: Host ISR `ufshcd_intr` acknowledged `UTRCS` (bit 0) and `UCCS` (bit 10) interrupts by writing `W1C` clears to `UFSHCI_IS` (`0x0020`).

### B. IMPLEMENTED (Hardware Emulation Added in QEMU)
1. **Protective MBR Synthesis (LBA 0)**: Injects signature `0xAA55` at byte offset `0x1FE` and Type `0xEE` partition entry pointing to LBA 1.
2. **GPT Header Synthesis (LBA 1)**: Injects signature `"EFI PART"`, Header Size `92`, Revision `1.0`, Backup LBA `131071`, and Partition Entry LBA `2`.
3. **Partition Entry Synthesis (LBA 2)**: Injects 128-byte partition entry for `"boot"` partition starting at LBA 34.
4. **Full SCSI Command Set**: `INQUIRY`, `READ CAPACITY (10)`, `READ (10)`, `READ (16)`, `TEST UNIT READY`, `MODE SENSE (6)`, `REPORT LUNS`, and `SYNCHRONIZE CACHE (10)`.

### C. INFERRED (Derived Architectural Next Steps)
1. **Block Partition Device Node Creation**: Linux kernel creates `/dev/sda` and `/dev/sda1` block device nodes upon receiving valid Protective MBR and `"EFI PART"` GPT header.
2. **Filesystem Probing**: Kernel storage subsystem probes filesystem magic bytes across `/dev/sda*` partitions.
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
UFS Host Controller           ⚡ IN PROGRESS Base 0x01D84000 | Protective MBR & GPT Header Synthesis
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
                                 ├── 14. Protective MBR (0xAA55) & GPT Header✓ IMPLEMENTED
                                 └── 15. Linux Block Partition Node Creation ← NEXT BLOCKER
```

---

## 4. Git Commits & Modified Files

- **`qemu_source` Repository**:
  - File: `hw/arm/qcom_ufshc.c`
  - Commit: `a86f784` ("Phase 10.47: Implement SCSI TEST UNIT READY (0x00) and MODE SENSE (6) (0x1A) handlers")
- **`CustomAndroidEmulator` Repository**:
  - File: `configs/phase1052_report.md`
  - Commit: Pending

---

## 5. Technical Roadmap to Next Milestone (Phase 10.53)

In Phase 10.53, we will verify Linux kernel partition registration (`/dev/sda1` through `sdaN`) as it reads the synthesized `"EFI PART"` GPT header and partition entries, tracking initial mount attempts for `/system` and `/vendor` toward `/init` execution.
