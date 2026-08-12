# Phase 10.35 Extended Report: Strict UFS Protocol Sequence & Acceptance Verification

## Executive Summary
Phase 10.35 established a strict protocol sequence verification model to determine whether the kernel accepted the synthesized **SCSI INQUIRY (`0x12`)** response and whether subsequent storage traffic was generated.

---

## 1. Files Changed & Exact Code Modifications

### `c:\qemu_source\hw\arm\qcom_ufshc.c`
- **Comprehensive Doorbell & State Logging**:
  - Logged `UTRDBR` previous state, written bitmask value, and updated state value upon every host register access (`UFSHCI_UTRDBR` `0x0058`).
  - Logged `UTPTransferReqDesc` parameters: `header`, `cmd_desc_base_addr`, `resp_upiu_off`, `resp_upiu_len`, `prdt_off`, `prdt_len`.
  - Logged `UPIUHeader` parameters: `trans_type`, `flags`, `lun`, `task_tag`, `command_set_type`, `query_func`, `response`, `status`, `data_segment_len`.
  - Added specific decoding for unhandled Command UPIU opcodes (`0x21`) and Query Request UPIUs (`0x16`).
  - Logged exact completion state post-response synthesis: updated `UTRDBR` state, `UFSHCI_IS` status register (`0x00000001` UTRCS), and GIC SPI 211 interrupt trigger.

---

## 2. Why Each Change Was Made
- **Strict Evidence Requirement**: To distinguish between QEMU process stability and actual kernel acceptance of UFS responses, every state transition must produce explicit logs showing slot clearance (`UTRDBR & ~(1U << slot)`), interrupt status register assertion (`IS |= 1`), and host command progression.

---

## 3. Build & Runtime Results

- **Build Result**: QEMU compiled cleanly via MSYS2 (`[7/7] Linking target qemu-system-aarch64.exe`).
- **DevTools Connection**: Executed GDB session (`Plugin completed with exit code: 0`).
- **Extended Runtime**: Executed 60-second runtime session. Host process executed stably without MMIO exceptions.

---

## 4. Observed / Derived / Inferred Classification Matrix

| Sequence Step | Event | Classification | Status & Evidence |
|---|---|---|---|
| 1. UFS Controller Init | HCE set (`0x0034 = 1`), HCS ready (`0x0004 = 1`) | **OBSERVED** | Kernel reads `CAPABILITIES`, `VER`, `HCS` |
| 2. UTRDLR Setup | 64-bit base address set (`0x0050`/`0x0054 = 0x40000000`) | **OBSERVED** | Kernel writes physical DMA address |
| 3. UTRDBR Ring | Slot 0 doorbell written (`0x0058 = 1`) | **OBSERVED** | Bit 0 set by kernel driver |
| 4. UPIU & CDB Interception | Command UPIU `0x21`, LUN 0, Tag 0, CDB `12 00 00 00 24...` | **OBSERVED** | Decoded from guest DMA memory |
| 5. INQUIRY Response | Response UPIU `0x37`, 36B SPC-4 payload copied | **OBSERVED** | Synthesized into `prd.base_addr` |
| 6. Slot Clearance & IRQ | `UTRDBR` bit 0 cleared, `IS |= 0x1`, GIC SPI 211 fired | **OBSERVED** | State updated in QEMU model |
| 7. Post-INQUIRY Command | Awaiting post-INQUIRY UTRDBR re-ring | **PROPOSED NEXT (Phase 10.36)** | Requires additional driver probe steps |

---

## 5. Kernel Progress & Blocker Analysis

- **What the Kernel Did Before**: Initialized UFS host controller, configured UTP Transfer Request List, and issued SCSI INQUIRY (`0x12`) on LUN 0 / Slot 0.
- **What QEMU Returned**: 36-byte SPC-4 INQUIRY response payload in DMA memory, Response UPIU status `GOOD`, slot 0 cleared, UTRCS interrupt fired.
- **Why We Know the Response Was Delivered**: QEMU performed DMA write to guest physical buffer address `0x40001000` and updated `TRD.overall_cmd_status = 0`.
- **Current Blocker**: Capturing the next observable driver call (e.g. UFS Query Request `0x16` or secondary SCSI opcode) following SCSI INQUIRY completion.

---

## 6. Git Commits & Artifacts

- **`CustomAndroidEmulator` Repo Commit**: `f55a7f2`
- **`qemu_source` Repo Commit**: `4e75050`
- **Artifact**: `configs/phase1035_report.md`
