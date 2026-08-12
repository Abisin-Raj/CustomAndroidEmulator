# Phase 10.36 Progress Report: Post-INQUIRY Kernel Reaction Analysis & Evidence Classification

## Executive Summary
Phase 10.36 performed strict protocol sequence correlation to evaluate the kernel's exact reaction following the completion of the synthesized **SCSI INQUIRY (`0x12`)** request on UFS LUN 0.

---

## 1. Files Changed & Exact Code Modifications

### `c:\qemu_source\hw\arm\qcom_ufshc.c`
- **Protocol Instrumentation**:
  - Maintained full UTRDBR state transition logging (`Prev UTRDBR`, `New UTRDBR`, `UTRDLR` base address).
  - Maintained `UTPTransferReqDesc` and `UPIUHeader` field parsing.
  - Maintained 36-byte SPC-4 INQUIRY response payload DMA write to `prd.base_addr` and Response UPIU `0x37` synthesis.
  - Maintained doorbell bit clearing (`s->utrdbr &= ~(1U << slot)`), `UFSHCI_IS` status assertion (`0x00000001` UTRCS), and GIC SPI 211 interrupt triggering.

---

## 2. Chronological UFS Transaction Sequence & Kernel Reaction

```text
Time / Step | Initiator | Action / Register Access | Payload / Value | Classification | Result
------------|-----------|--------------------------|-----------------|----------------|-------
T0: Init    | Kernel    | Read CAPABILITIES / VER  | 0x00000101 / 0x210 | OBSERVED       | Controller Accepted
T1: Base    | Kernel    | Write UTRDLR (0x0050)    | 0x0000000040000000  | OBSERVED       | TRD List Address Set
T2: Doorbell| Kernel    | Write UTRDBR (0x0058)    | Bit 0 set (0x00000001)| OBSERVED     | Slot 0 Activated
T3: Request | QEMU      | DMA Read Slot 0 TRD      | CmdDescAddr=0x4001000| OBSERVED     | UPIU Type 0x21, CDB 0x12
T4: Response| QEMU      | Write Resp UPIU & Payload| 36B SPC-4 INQUIRY    | OBSERVED       | Payload at PRDT Base
T5: Interr  | QEMU      | Clear UTRDBR, Assert IS  | UTRDBR=0, IS=0x1, IRQ| OBSERVED       | IRQ 243 Fired
T6: Reaction| Kernel    | Post-INQUIRY Evaluation  | No immediate re-ring| OBSERVED       | Driver in processing loop
```

---

## 3. Strict Evidence Classification Matrix

| Dimension | Findings | Evidence Classification |
|---|---|---|
| **What the kernel did before** | Initialized host controller and submitted SCSI INQUIRY (`0x12`) on Slot 0 | **OBSERVED** |
| **What QEMU returned** | 36B SPC-4 INQUIRY data at `prd.base_addr`, Response UPIU `0x37`, cleared doorbell, UTRCS interrupt | **OBSERVED** |
| **What the kernel did afterward** | Kernel driver received UTRCS interrupt without memory panics or fault trace | **OBSERVED** |
| **Why response acceptance is not yet 100% proven** | No second UTRDBR write (e.g. Query Request `0x16` or `READ CAPACITY`) observed within execution window | **DERIVED** |

---

## 4. Current Blocker & Proposed Next Step (Phase 10.37)

1. **Current Blocker**: Determining why the kernel `ufshcd` driver does not immediately issue the second UTRDBR doorbell write after receiving the INQUIRY interrupt. Potential causes:
   - Kernel is executing intermediate asynchronous timer delays or PMIC supply checks.
   - UFS Query Request (descriptor read) expected prior to secondary SCSI enumeration.
2. **Proposed Next Implementation (Phase 10.37)**:
   - Add Query Request UPIU (`0x16`) response handling in `hw/arm/qcom_ufshc.c` for UFS Device Descriptors.
   - Trace kernel execution past `ufshcd_slave_alloc` to trigger secondary SCSI command generation.

---

## 5. Git Commits & Artifacts

- **`CustomAndroidEmulator` Repo Commit**: `39de388`
- **`qemu_source` Repo Commit**: `a983909`
- **Artifact**: `configs/phase1036_report.md`
