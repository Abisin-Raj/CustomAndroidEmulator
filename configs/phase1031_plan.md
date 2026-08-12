# Phase 10.31 Plan: UFS Active Storage Command Contract & Image Backing

## Objective
Transition UFS from a probe-level placeholder to a functional UFS Host Controller capable of processing UPIU transfer requests, SCSI LUN discovery, and partition table access.

---

## Technical Workflow & Steps

```text
1. Trace UFS Doorbell Writes
        ↓
2. Intercept UTP Transfer Request List (UTRDLR @ 0x0050, UTRDBR @ 0x0054)
        ↓
3. Decode NOP OUT & SCSI Command UPIUs
        ↓
4. Inject Minimal UPIU Response / Inquiry Data (LUN 0 discovery)
        ↓
5. Wire Back-end Android Storage Image (PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img)
        ↓
6. Verify SCSI Registration & GPT Partition Table Reading
```

---

## Proposed Changes

### QEMU UFS Device Model
- **File**: `c:\qemu_source\hw\arm\qcom_ufshc.c`
- **Updates**:
  - Add DMA reading of UTP Transfer Request Descriptors (UTRD).
  - Add SCSI Inquiry & Read Capacity responses for LUN 0.
  - Implement completion interrupt firing (`s->is |= UTRCS`).

---

## Verification Plan

1. **Build Verification**: Compile updated `qcom_ufshc.c` in QEMU.
2. **GDB & Trace Verification**: Run DevTools plugin `phase1031` against live QEMU stub to trace UFS command submission.
3. **Storage Verification**: Confirm SCSI host registration and LUN 0 detection.
