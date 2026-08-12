# Phase 10.31 Report: UFS Passive Storage Command Tracing

## Target Summary
Interception and decoding of active UFS storage commands (UTP Transfer Request List Base Address `0x0050`, UTRDBR `0x0054`, UPIU transaction descriptors, and SCSI Command Descriptor Blocks).

---

## 1. Active UFS Command Architecture

In the Qualcomm UFSHCI v2.1 spec:
1. The kernel configures `UFSHCI_UTRDLR_LOW` (`0x0050`) & `UFSHCI_UTRDLR_HIGH` (`0x0054`) with the 64-bit physical DMA address of the UTP Transfer Request Descriptor list.
2. The kernel rings `UFSHCI_UTRDBR` (`0x0058`) with a slot bitmask (e.g. bit 0 for slot 0).
3. Each descriptor (`UTPTransferReqDesc`, 32 bytes) contains `cmd_desc_base_addr` pointing to the Request UPIU and 16-byte SCSI CDB.

```text
Host Kernel
  │
  ├── Writes 64-bit UTRDLR (0x0050/0x0054)
  └── Writes UTRDBR Doorbell (0x0058)
        │
        ▼
   QEMU UFS Host Controller (qcom-ufshc @ 0x01D84000)
        │
        ├── DMA Read UTP Transfer Request Descriptor (32B)
        ├── Extract Command Descriptor Base Address
        ├── Read UPIU Header (Type 0x21 Command UPIU)
        └── Extract 16-byte SCSI CDB (Command Descriptor Block)
```

---

## 2. Hardware Register Mapping Verified

| Offset | Register Name | Access | Logged Semantics |
|---|---|---|---|
| `0x0050` | `UFSHCI_UTRDLR_LOW` | R/W | UTP Transfer Request List Base Address Low |
| `0x0054` | `UFSHCI_UTRDLR_HIGH` | R/W | UTP Transfer Request List Base Address High |
| `0x0058` | `UFSHCI_UTRDBR` | R/W | UTP Transfer Request Doorbell Register |
| `0x0070` | `UFSHCI_UTMRLR_LOW` | R/W | Task Management List Base Address Low |
| `0x0074` | `UFSHCI_UTMRLR_HIGH` | R/W | Task Management List Base Address High |
| `0x0078` | `UFSHCI_UTMDBR` | R/W | Task Management Request Doorbell |

---

## 3. Backing Storage Image Verification

Checked workspace image assets:
- Image Path: `C:\Users\ABISIN RAJ\Desktop\Projects\CustomAndroidEmulator\PixelExperience_violet-13.0-20231124-0006-OFFICIAL.img`
- Image Size: 67,108,864 bytes (64 MB extracted raw partition / ramdisk image)
- Compressed ROM Archive: `PixelExperience_violet-13.0-20240127-0711-OFFICIAL.zip` (1.99 GB)

---

## 4. Next Milestone (Phase 10.31 Continuation & Response Synthesis)

With UTRD/UPIU DMA extraction logic integrated into `hw/arm/qcom_ufshc.c`:
1. Synthesize minimal response UPIU for the first captured SCSI command (e.g. INQUIRY / READ CAPACITY).
2. Complete UTP Transfer Request Status (`UTPTransferReqDesc.overall_cmd_status = 0`) and clear UTRDBR doorbell bit.
3. Fire UFS completion interrupt (`s->is |= 0x01`, `qemu_set_irq(s->irq, 1)`).
4. Verify kernel SCSI host registration (`scsi host0: ufshcd`).
