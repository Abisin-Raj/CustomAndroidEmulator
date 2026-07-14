# Storage Compatibility Analysis

**ROM:** PixelExperience_violet-13.0-20240127-0711-OFFICIAL  
**Device:** Xiaomi Redmi Note 7 Pro (violet) — Snapdragon 675, eMMC storage  
**Updated:** Phase 6.2 (ongoing)

---

## Known Storage Controllers in Kernel

| Controller | Driver | Kernel Status | Expected DT Node | QEMU Equivalent | Notes |
|---|---|---|---|---|---|
| Qualcomm SDHCI-MSM (eMMC) | `CONFIG_MMC_SDHCI_MSM` | ✅ Built-in | `sdhci@7c4000` (approx.) | ❓ Unknown | Primary root storage on violet |
| Qualcomm UFS | `CONFIG_SCSI_UFSHCD` / `CONFIG_SCSI_UFS_QCOM` | ✅ Built-in | `ufshc@...` | ❓ Unknown | Not used on violet (uses eMMC) |
| VirtIO block | N/A | ❌ Not compiled | N/A | `virtio-blk` | **Cannot use** — no VirtIO bus in kernel |
| RAM disk | `CONFIG_BLK_DEV_RAM` | ✅ Built-in | N/A | `-initrd` | Available but not the native path |
| Loop device | `CONFIG_BLK_DEV_LOOP` | ✅ Built-in | N/A | N/A | Available, requires initramfs to set up |

---

## DTB Triple-Check Rule

Before any DTB-based compatibility solution is attempted, all three must align:

1. **Does the kernel have a driver?** (check `CONFIG_*` above)
2. **Does the DTB describe the hardware?** (check `dtbo.img` + device tree source)
3. **Does QEMU actually emulate it?** (check QEMU `-M virt` device model documentation)

A DTB entry describes hardware. It does not create it.

---

## Boot Chain Evidence

### Kernel cmdline (from `boot.img`)
```
androidboot.hardware=qcom
androidboot.console=ttyMSM0
service_locator.enable=1    swiotlb=1
earlycon=msm_geni_serial,0x880000
loop.max_part=7             kpti=off
androidboot.vbmeta.avb_version=1.0
buildvariant=userdebug
```

**Notable absences:** `root=`, `skip_initramfs`, `rootwait`, `rootfstype`, `dm=`

### Kernel config implications
```
CONFIG_CMDLINE_EXTEND=y          → kernel only appends to DTB cmdline; root= injection may not work
# CONFIG_CMDLINE_FORCE is not set → bootloader cmdline is authoritative
# CONFIG_DEVTMPFS is not set      → /dev must be populated by ueventd (first-stage init)
CONFIG_OF=y                      → hardware described entirely via Device Tree
# CONFIG_BUILD_ARM64_DT_OVERLAY is not set → DTB must be correct upfront; no runtime overlay
```

### Current hypothesis
The kernel expects the Qualcomm SDHCI-MSM controller declared in its Device Tree at the MMIO address `0x07c4000` (probed by the `7c4000.sdhci` platform device driver). QEMU's `virt` board does not expose this controller, so no `/dev/block/platform/soc/7c4000.sdhci` is ever created, causing the VFS boot panic.

---

## Phase 6.2 Targets — Filesystem & Recovery Inspection

### Recovery Image fstab & Init Analysis (Completed)
- [x] `/system/etc/recovery.fstab` — primary fstab located inside `recovery.img`.
- [x] `/init.recovery.qcom.rc` — qcom specific recovery scripts.

**Verified Mount Configuration:**
- **System Mount Point:** `/` (System-As-Root)
- **Source Device:** `/dev/block/bootdevice/by-name/system` (ext4)
- **Vendor Mount Point:** `/vendor`
- **Source Device:** `/dev/block/bootdevice/by-name/vendor` (ext4)
- **Platform Device Node Resolution:**
  In `init.recovery.qcom.rc`:
  ```rc
  wait /dev/block/platform/soc/${ro.boot.bootdevice}
  symlink /dev/block/platform/soc/${ro.boot.bootdevice} /dev/block/bootdevice
  ```
  `vendor_file_contexts` confirms the absolute platform block path:
  `/dev/block/platform/soc/7c4000.sdhci/by-name/...`
  Thus, `ro.boot.bootdevice` is explicitly **`7c4000.sdhci`** (eMMC controller at `0x07c4000`).

### Reconstructed system/vendor filesystem analysis (Pending system.img generation)
- [ ] `/vendor/etc/fstab.qcom` — primary target system fstab
- [ ] `/vendor/etc/fstab.violet` — device-specific override (if present)
- [ ] `/init` binary & `/init.rc` — first-stage init entrypoint & configurations
- [ ] `ueventd.rc`, `ueventd.violet.rc` — device node creation rules

---

## Phase 6.3 Research Questions

**Success criterion:** We know *exactly* why the kernel cannot see a storage device — not necessarily that we have solved it.

1. What MMIO address range does the kernel probe for Qualcomm SDHCI-MSM? (Probed: `0x07c4000` via device tree `7c4000.sdhci`).
2. Does QEMU's `virt` board expose any MMIO region at `0x07c4000`?
3. Is there any QEMU device model that registers at a compatible address or could be mapped?
4. Could a custom DTB redirect the SDHCI driver to a QEMU-backed MMIO region?
5. If not: what is the minimum kernel change or alternative transport (e.g. initramfs/loop/ramdisk) that can mount system.img?

---

## Open Findings Log

| Phase | Finding | Impact |
|---|---|---|
| 6.1 | `ramdisk_size=0` in boot.img v1 | Confirms system-as-root; no embedded initramfs |
| 6.1 | No VirtIO in kernel config | VirtIO block device approach is closed |
| 6.1 | `CONFIG_CMDLINE_EXTEND=y` | `root=` injection via `-append` may be ignored |
| 6.1 | No `CONFIG_DEVTMPFS` | Even successful root mount needs full init chain for `/dev` |
| 6.2 | Recovery fstab system path | Root mounts `/dev/block/bootdevice/by-name/system` |
| 6.2 | Boot device resolved | `ro.boot.bootdevice` = `7c4000.sdhci` |
| 6.3 | SDHCI-MSM MMIO address | eMMC mapped to `0x07c4000` (Qualcomm platform device) |

