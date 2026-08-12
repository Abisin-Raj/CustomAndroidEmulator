# Phase 10.46 Detailed Brief: Partition Device Creation Observation & Storage Layer Trace

## Executive Summary
Phase 10.46 evaluated the Linux SCSI block layer (`sd_probe`) and partition table evaluation stage in `hw/arm/qcom_ufshc.c`, tracing high-sector data reads (`LBA > 33`) to verify whether Linux parses returned GPT partition entries to create block device nodes (`/dev/block/sda*`).

---

## 1. Categorized Evidence Matrix

### A. OBSERVED (Directly Verified Runtime Traces)
1. **Controller & Link Setup**: Host initializes UFSHCI registers, configures `IE` (`0x0030`), sets `UTRDLR` base (`0x0050/0x0054`), and executes UIC link commands (`0x0090`).
2. **SCSI Protocol Handshake**: Host submits SCSI INQUIRY (`0x12`), READ CAPACITY (`0x25`), and READ(10) (`0x28`) on Slot 0.
3. **ROM File Sector DMA**: QEMU reads sector bytes from `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img` and performs DMA transfers into guest PRDT memory buffers.
4. **Header Signature Verification**: LBA 0 sector DMA returns `"ANDROID!"` (`41 4E 44 52 4F 49 44 21`), matching Android boot image / partition sector data.
5. **Interrupt Servicing**: Host ISR `ufshcd_intr` acknowledges `UTRCS` and `UCCS` interrupts by writing `W1C` clears to `UFSHCI_IS` (`0x0020`).

### B. IMPLEMENTED (Hardware Emulation Added in QEMU)
1. **SCSI READ CAPACITY (10) (`0x25`)**: Returns Last LBA `131,071` (`0x0001FFFF`) and 512-byte sector size (`0x00000200`).
2. **SCSI READ (10) (`0x28`) File Backing**: Attached `PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img` to serve sector reads via DMA.
3. **Query Request UPIU (`0x16`)**: Synthesizes 64B Device Descriptor (IDN `0x00`) and 48B Geometry Descriptor (IDN `0x07`).
4. **UIC Link Completion**: Asserts `UCCS` (bit 10) in `UFSHCI_IS` on `UICCMD` writes to complete Unipro link setup.
5. **Partition Scan Tracing**: Added LBA 0-33 and high-sector `[UFS-DATA-READ]` logging to track partition device node creation.

### C. INFERRED (Derived Architectural Next Steps)
1. **Partition Table Evaluation**: Linux kernel SCSI block layer (`sd_probe`) processes returned GPT partition table entries to register `/dev/block/sda*` partition devices.
2. **Mount Operations**: Android init/vold subsystem mounts `/system`, `/vendor`, and `/product` partitions from registered block nodes.
3. **Init Process Execution**: Linux kernel launches `/init` process from mounted root filesystem.

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
UFS Host Controller           ⚡ IN PROGRESS Base 0x01D84000 | Sector File-I/O & Block Device Node Creation
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
                                 ├── 11. Sector File DMA Transfer ("ANDROID!") ✓ OBSERVED
                                 └── 12. Linux Block Partition Node Registration ← NEXT BLOCKER
```

---

## 3. Git Commits & Artifacts
- **`CustomAndroidEmulator` Repo Commit**: `d2880c8`
- **`qemu_source` Repo Commit**: `8606e89`
- **Artifact**: `configs/phase1046_report.md`

---

## 4. Technical Roadmap to Next Milestone (Phase 10.47)

In Phase 10.47, we will observe the exact Linux kernel partition table parse completion, verifying the registration of partition block devices (`/dev/block/sda1` through `sdaN`) and tracking the kernel's first mount attempts for `/system` and `/vendor` toward `/init` execution.
