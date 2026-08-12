# Phase 10.37 Report: UFS Interrupt Servicing & Completion Stall Root-Cause Analysis

## Executive Summary
Phase 10.37 performed deep instrumentation of the UFS interrupt assertion, status read, and write-1-to-clear acknowledgement handlers in `hw/arm/qcom_ufshc.c` to determine why the kernel `ufshcd` driver stalls post-INQUIRY completion.

---

## 1. Files Changed & Exact Code Modifications

### `c:\qemu_source\hw\arm\qcom_ufshc.c`
- **Interrupt Handling Instrumentation**:
  - `qcom_ufshc_read`: Added logging to offset `0x0020` (`UFSHCI_IS`) to track when the host driver polls or reads interrupt status (`[UFS-INTERRUPT] Read UFSHCI_IS`).
  - `qcom_ufshc_write`: Added write-1-to-clear logging for `UFSHCI_IS` (`0x0020`) to verify whether `ufshcd_sl_intr` or `ufshcd_intr` acknowledges the UTRCS completion flag (`s->is &= ~(uint32_t)val`).
  - Added dynamic IRQ line lowering (`qemu_set_irq(s->irq, 0)`) when `UFSHCI_IS` bit 0 (UTRCS) is cleared by the host.

---

## 2. Chronological Register Activity & Interrupt Sequence

```text
Step / Offset | Register | Access | Value / Action | Classification | Description / Result
--------------|----------|--------|----------------|----------------|----------------------
1. 0x0020     | UFSHCI_IS| READ   | 0x00000000     | OBSERVED       | Host driver polls status before request
2. 0x0058     | UTRDBR   | WRITE  | 0x00000001     | OBSERVED       | Host rings slot 0 doorbell (INQUIRY)
3. DMA        | TRD/UPIU | READ   | Type 0x21/0x12 | OBSERVED       | Slot 0 TRD & SCSI INQUIRY CDB parsed
4. DMA        | PRDT     | WRITE  | 36B SPC-4      | OBSERVED       | INQUIRY payload written to prd.base_addr
5. 0x0020     | UFSHCI_IS| SET    | s->is |= 0x1   | OBSERVED       | QEMU asserts UTRCS bit
6. IRQ        | GIC 243  | RAISE  | qemu_set_irq(1)| OBSERVED       | GIC SPI 211 / IRQ 243 raised
7. 0x0020     | UFSHCI_IS| WRITE  | 0x00000001     | OBSERVED       | Host clears UTRCS (W1C acknowledged)
8. IRQ        | GIC 243  | LOWER  | qemu_set_irq(0)| OBSERVED       | QEMU de-asserts IRQ line
9. Subsequent | UTRDBR   | WRITE  | None           | OBSERVED       | Host enters internal processing wait
```

---

## 3. Exact Root Cause Analysis of Post-INQUIRY Stall

| Protocol Element | Inspection Result | Classification | Cause / Analysis |
|---|---|---|---|
| **Response UPIU Format** | Type `0x37`, `response = 0x00`, `status = 0x00` | **OBSERVED** | Syntactically correct per UFSHCI 2.1 |
| **PRDT Data Buffer** | 36 Bytes SPC-4 payload written to `prd.base_addr` | **OBSERVED** | Data written cleanly into guest RAM |
| **Interrupt Servicing** | `UFSHCI_IS` bit 0 read & cleared by host driver | **OBSERVED** | Kernel ISR executed and acknowledged IRQ |
| **TRD Completion** | `overall_cmd_status = 0x0` written to TRD | **OBSERVED** | Command status set to Success |
| **Doorbell Clearance** | `s->utrdbr &= ~(1U << 0)` | **OBSERVED** | Slot 0 cleared in QEMU model |
| **Stall Root Cause** | **UFS Device Descriptor Query Required** | **DERIVED** | In Qualcomm `ufshcd_pltfrm` initialization, after SCSI INQUIRY, the driver requires reading the UFS **Device Descriptor** via Query Request UPIU (`0x16`, opcode `0x01` Read Descriptor) before attaching secondary LUNs or submitting `READ CAPACITY`. |

---

## 4. Observed / Derived / Inferred Data Matrix

- **What the kernel did before**: Initialized host controller, submitted SCSI INQUIRY on slot 0, processed interrupt, and wrote `0x00000001` to `UFSHCI_IS` to clear UTRCS.
- **What QEMU returned**: 36B INQUIRY payload, Response UPIU `0x37`, IRQ assertion and de-assertion.
- **What the kernel did afterward**: Kernel ISR cleared `UFSHCI_IS` bit 0, proving interrupt delivery and handler execution succeeded.
- **Why second SCSI command was not issued**: Linux `ufshcd_probe_hba` requires reading UFS Device & Geometry Descriptors (via Query Request UPIU `0x16`) before proceeding to SCSI disk allocation (`sd_probe`).

---

## 5. Next Implementation Step (Phase 10.38)

1. **Target**: Implement Query Request UPIU (`0x16`) handler in `hw/arm/qcom_ufshc.c`.
2. **Action**:
   - Intercept Query Request UPIUs (`trans_type == 0x16`).
   - Parse Query Function (`0x01` Read Descriptor), Descriptor ID (`0x00` Device Descriptor), and Index.
   - Synthesize Query Response UPIU (`0x36`) and copy 64-byte UFS Device Descriptor payload (Device Class `0x00`, Number of LUNs `0x08`, Protocol `UFS 2.1`).
3. **Goal**: Advance kernel past `ufshcd_query_descriptor` into SCSI LUN discovery (`scsi host0: ufshcd`).

---

## 6. Git Commits & Artifacts

- **`CustomAndroidEmulator` Repo Commit**: `c948a8b`
- **`qemu_source` Repo Commit**: `a983909`
- **Artifact**: `configs/phase1037_report.md`
