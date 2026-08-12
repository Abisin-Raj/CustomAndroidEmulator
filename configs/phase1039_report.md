# Phase 10.39 Report: Deep UIC Link & Post-INQUIRY Host Protocol Tracing

## Executive Summary
Phase 10.39 expanded UFS register instrumentation in `hw/arm/qcom_ufshc.c` to trace UIC (Unipro Interconnect) link command writes (`0x0090` `UICCMD`) and unhandled register accesses, investigating the exact host state machine post-INQUIRY completion.

---

## 1. Files Changed & Code Modifications

### `c:\qemu_source\hw\arm\qcom_ufshc.c`
- **UIC Interconnect & Interrupt Register Additions**:
  - `0x0030` (`UFSHCI_IE` Interrupt Enable): Added read handler returning `0xFFFFFFFF` (all interrupts enabled).
  - `0x0090` (`UFSHCI_UICCMD` UIC Command): Added write handler to intercept UIC link management commands, setting `UFSHCI_IS` bit 10 (`UCCS` - UIC Command Completion Status) and raising GIC IRQ 243.
  - `0x00A0` (`UFSHCI_UICCMDARG3` / UIC Command Status): Added read handler returning `0x00000000` (UIC Command Success / Link Ready).
  - **Unhandled Access Tracing**: Added fallback `fprintf` logging for all reads/writes to unmapped UFSHCI offsets to catch unexpected host driver register accesses.

---

## 2. Chronological Register & Link Activity Matrix

```text
Step | Offset / Region | Operation | Value / Structure | Classification | Result
-----|-----------------|-----------|-------------------|----------------|-------
T0   | 0x0034 / 0x0004 | Init      | HCE=1, HCS=1       | OBSERVED       | Host Controller Ready
T1   | 0x0030          | Config    | IE=0xFFFFFFFF     | OBSERVED       | Interrupts Enabled
T2   | 0x0050 / 0x0054 | Config    | UTRDLR=0x40000000 | OBSERVED       | Descriptor List Base Set
T3   | 0x0058          | Doorbell  | UTRDBR Write=0x1  | OBSERVED       | Slot 0 Doorbell Rung
T4   | DMA Read        | TRD / UPIU| Type=0x21, Opcode=0x12 | OBSERVED  | SCSI INQUIRY Intercepted
T5   | DMA Write       | Response  | 36B SPC-4 Payload | OBSERVED       | Wrote Payload to PRDT Base
T6   | 0x0020 / IRQ    | Interrupt | IS |= 0x1, IRQ 243| OBSERVED       | Asserted UTRCS Interrupt
T7   | 0x0020          | W1C Clear | Write 0x1 to IS   | OBSERVED       | Host Cleared Interrupt (Acknowledged)
T8   | 0x0090 / 0x00A0 | UIC Link  | UICCMD & Status   | OBSERVED       | Intercepted UIC Command (UCCS Asserted)
```

---

## 3. Evidence Classification Matrix

| Dimension | Observation / Finding | Classification |
|---|---|---|
| **What the kernel did before** | Initialized controller, set `IE` & `UTRDLR`, submitted SCSI INQUIRY on Slot 0 | **OBSERVED** |
| **What QEMU returned** | Synthesized 36B INQUIRY payload, Response UPIU `0x37`, asserted `UTRCS`, de-asserted on W1C clear | **OBSERVED** |
| **What the kernel did afterward** | Kernel ISR acknowledged `UTRCS` and executed UIC link management calls (`0x0090`) | **OBSERVED** |
| **Why next command is pending** | Host kernel driver executes UIC link power mode / lane configuration prior to issuing secondary SCSI CDBs | **DERIVED** |

---

## 4. Current Blocker & Proposed Next Step (Phase 10.40)

1. **Current Blocker**: Observing the first post-INQUIRY storage command (e.g. UFS Query Request `0x16` or SCSI `READ CAPACITY` `0x25`).
2. **Next Implementation Step**:
   - Maintain UIC link completion handling (`UCCS` bit 10).
   - Capture the next slot ring on `UTRDBR` (`0x0058`) and decode its transaction type.
   - Prepare image backing (`PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img`) to serve block reads as soon as `READ CAPACITY` / `READ (10)` is requested.

---

## 5. Git Commits & Artifacts

- **`CustomAndroidEmulator` Repo Commit**: `9bf1607`
- **`qemu_source` Repo Commit**: `a983909`
- **Artifact**: `configs/phase1039_report.md`
