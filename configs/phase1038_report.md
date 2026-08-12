# Phase 10.38 Report: UFS Response Format Verification & Protocol Sequence Trace

## Executive Summary
Phase 10.38 verified the exact layout of the synthesized **SCSI INQUIRY (`0x12`)** Response UPIU (`Type 0x37`) and PRDT payload against the kernel's completion path, verifying byte-order formatting (`data_segment_len_lo = cpu_to_be16(36)`) and offset calculations.

---

## 1. Files Changed & Exact Code Modifications

### `c:\qemu_source\hw\arm\qcom_ufshc.c`
- **Response UPIU Format Verification**:
  - Configured `data_segment_len_lo = cpu_to_be16(36)` in the Response UPIU Header (`Type 0x37`).
  - Added explicit offset logging for `resp_addr = trd.cmd_desc_base_addr + (trd.resp_upiu_off * 4)` and `prdt_addr = trd.cmd_desc_base_addr + (trd.prdt_off * 4)`.
  - Maintained `UFSHCI_IS` read (`0x0020`) and write-1-to-clear acknowledgement logging (`[UFS-INTERRUPT] Write UFSHCI_IS Clear`).
  - Maintained dynamic IRQ line lowering (`qemu_set_irq(s->irq, 0)`).

---

## 2. Chronological UFS Register & Interrupt Sequence

```text
Step | Offset / Region | Operation | Value / Structure | Classification | Result
-----|-----------------|-----------|-------------------|----------------|-------
T0   | 0x0034 / 0x0004 | Init      | HCE=1, HCS=1       | OBSERVED       | Controller Enabled
T1   | 0x0050 / 0x0054 | Config    | UTRDLR=0x40000000 | OBSERVED       | List Base Address Set
T2   | 0x0058          | Doorbell  | UTRDBR Write=0x1  | OBSERVED       | Slot 0 Activated
T3   | DMA Read        | TRD       | CmdDescAddr=0x4001000 | OBSERVED   | Slot 0 TRD Header Parsed
T4   | DMA Read        | UPIU/CDB  | Type=0x21, Opcode=0x12 | OBSERVED  | SCSI INQUIRY Intercepted
T5   | DMA Write       | Response  | Resp UPIU (0x37) at 0x4001000 | OBSERVED | Header written to resp_addr
T6   | DMA Write       | Payload   | 36B SPC-4 Payload at 0x4001020| OBSERVED | Data transferred to prd.base_addr
T7   | DMA Write       | TRD Status| overall_cmd_status=0x0 | OBSERVED | Slot 0 TRD marked Success
T8   | 0x0058 / 0x0020 | State     | UTRDBR=0, IS |= 0x1, IRQ 243 | OBSERVED | IRQ Asserted
T9   | 0x0020          | W1C Clear | Write 0x00000001 to UFSHCI_IS | OBSERVED | Host ISR Clears IRQ (De-asserted)
```

---

## 3. Evidence Classification Matrix

| Dimension | Observation / Result | Classification |
|---|---|---|
| **What the kernel did before** | Initialized controller, set UTRDLR base, and submitted SCSI INQUIRY on Slot 0 | **OBSERVED** |
| **What QEMU returned** | 36B SPC-4 INQUIRY payload, Response UPIU `0x37` with `data_segment_len = 36`, cleared doorbell, raised IRQ | **OBSERVED** |
| **What the kernel did afterward** | Host driver executed `ufshcd_intr` and wrote `0x00000001` to `UFSHCI_IS` to clear UTRCS | **OBSERVED** |
| **Why next transaction is pending** | Host kernel driver enters internal probe sequence prior to issuing next doorbell write | **DERIVED** |

---

## 4. Current Blocker & Proposed Next Implementation (Phase 10.39)

1. **Current Blocker**: Observing the first post-INQUIRY host request (e.g. UFS Query Request UPIU `0x16` for Device Descriptor or secondary SCSI opcode).
2. **Next Implementation Step**:
   - Trace the exact slot and transaction type of the next host doorbell ring.
   - Synthesize the corresponding response payload upon observation.

---

## 5. Git Commits & Artifacts

- **`CustomAndroidEmulator` Repo Commit**: `d58bf99`
- **`qemu_source` Repo Commit**: `a983909`
- **Artifact**: `configs/phase1038_report.md`
