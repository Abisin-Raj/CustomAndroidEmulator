# Guest Hardware Dependency Map (Phase 1.2)

**Target Platform:** Qualcomm Snapdragon 675 / SM6150 (Xiaomi Redmi Note 7 Pro `violet`)  
**Purpose:** Map every unmodeled or bypassed hardware subsystem from guest userland down through the kernel driver to expected Qualcomm silicon and the current QEMU status.

---

## 1. Subsystem Dependency Chains

```
+--------------------------------------------------------------------------------------------------------------------+
| 1. ADRENO 612 GPU & KGSL SUBSYSTEM                                                                                 |
+--------------------------------------------------------------------------------------------------------------------+
Guest Component:          ANGLE / Vulkan Loader (libvulkan.so) / Vendor EGL HAL (vulkan.adreno.so / libGLESv2_adreno)
                                ↓
Userspace Interface:      /dev/kgsl-3d0  (character device)
Kernel Interface:         ioctl(IOCTL_KGSL_DEVICE_GETPROPERTY, IOCTL_KGSL_GPUOBJ_ALLOC, IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS)
                                ↓
Kernel Driver:            drivers/gpu/msm/kgsl.c, kgsl_adreno.c, adreno_a6xx.c
                                ↓
Qualcomm Hardware:        Qualcomm Adreno 612 GPU (A6xx family, qcom,adreno-612)
                          - MMIO range: 0x05090000 - 0x050a0000 (GPU core, CP, RBBM, GMU)
                          - Interrupts: GPU GX IRQ (GIC 300), GMU IRQ (GIC 304)
                          - Clocks: GCC_GPU_CFG_AHB_CLK, GPU_CC_GX_GFX3D_CLK
                                ↓
Current QEMU Status:      NOT IMPLEMENTED (0x0 MMIO, no DT node)
Missing Functionality:    Device node creation, GPU register read/write, ringbuffer submission, fence completion
```

```
+--------------------------------------------------------------------------------------------------------------------+
| 2. MDSS / DPU (MOBILE DISPLAY SUBSYSTEM / DISPLAY PROCESSING UNIT)                                                 |
+--------------------------------------------------------------------------------------------------------------------+
Guest Component:          SurfaceFlinger / android.hardware.graphics.composer@2.3-service (HWC2/HWC3)
                                ↓
Userspace Interface:      /dev/dri/card0 (Direct Rendering Manager)
Kernel Interface:         DRM/KMS ioctls: DRM_IOCTL_MODE_GETRESOURCES, DRM_IOCTL_MODE_ATOMIC_COMMIT, PAGE_FLIP
                                ↓
Kernel Driver:            drivers/gpu/drm/msm/disp/dpu1/dpu_kms.c, sde_kms.c
                                ↓
Qualcomm Hardware:        Qualcomm DPU 5.1 / MDSS (qcom,sde-kms, qcom,mdss)
                          - MMIO range: 0x0ae00000 (MDSS top), 0x0ae01000 (MDP/DPU), 0x0ae94000 (DSI-0)
                          - Interrupts: MDSS IRQ (GIC 83)
                          - Hardware pipes: ViG (Video Graphics), DMA, SSPP layers, mixer, pingpong, encoder
                                ↓
Current QEMU Status:      NOT IMPLEMENTED (Replaced by QEMU ramfb @ 1080x2340 linear framebuffer)
Missing Functionality:    Hardware CRTC modesetting, multi-layer hardware overlay composition, DSI scanout
```

```
+--------------------------------------------------------------------------------------------------------------------+
| 3. DISPLAY VSYNC SOURCE / HARDWARE VBLANK GENERATOR                                                                |
+--------------------------------------------------------------------------------------------------------------------+
Guest Component:          SurfaceFlinger (EventThread / Scheduler) / HWC onVsync() callback
                                ↓
Userspace Interface:      DRM VBLANK event queue (DRM_IOCTL_WAIT_VBLANK) / sysfs vsync event node
                                ↓
Kernel Driver:            drivers/gpu/drm/msm/msm_drv.c, dpu_encoder.c, sde_encoder_phys_vid.c
                                ↓
Qualcomm Hardware:        MDSS Timing Engine / INTF_0 VBLANK interrupt generator
                          - Interrupt: INTF_0_VSYNC (line pulse at 60.00 Hz)
                                ↓
Current QEMU Status:      SYNTHETIC BYPASS (SurfaceFlinger internal software timer generates 60 Hz ticks)
Missing Functionality:    Hardware-synchronized frame presentation interrupts
```

```
+--------------------------------------------------------------------------------------------------------------------+
| 4. GRAPHICS MEMORY & BUFFER ALLOCATOR (GRALLOC / ION / DMA-BUF)                                                    |
+--------------------------------------------------------------------------------------------------------------------+
Guest Component:          android.hardware.graphics.allocator@4.0-service / libui GraphicBuffer
                                ↓
Userspace Interface:      /dev/ion / /dev/dma_heap/system / /dev/hwbinder (IAllocator)
Kernel Interface:         ION_IOC_ALLOC, DMA_BUF_IOCTL_SYNC, mmap()
                                ↓
Kernel Driver:            drivers/staging/android/ion/ion.c, msm_ion.c, dma-buf.c
                                ↓
Qualcomm Hardware:        Qualcomm ION carveouts + APPS SMMU (System MMU) context banks
                                ↓
Current QEMU Status:      PARTIALLY IMPLEMENTED (Compact ION carveout in DTB, but without SMMU translation)
Missing Functionality:    Hardware SMMU address translation for GPU/DPU zero-copy access
```

```
+--------------------------------------------------------------------------------------------------------------------+
| 5. SYSTEM MMU (APPS SMMU / ARM SMMU-v2/v3)                                                                         |
+--------------------------------------------------------------------------------------------------------------------+
Guest Component:          Kernel DMA subsystem / KGSL / MDSS drivers
                                ↓
Kernel Interface:         struct iommu_domain, iommu_map(), iommu_unmap(), arm_smmu_domain
                                ↓
Kernel Driver:            drivers/iommu/arm-smmu.c, qcom_iommu.c
                                ↓
Qualcomm Hardware:        Qualcomm APPS SMMU (qcom,qsmmu-v500 / ARM SMMU-v2)
                          - MMIO range: 0x15000000 (SMMU global registers, context banks 0-31)
                          - Interrupts: Global fault IRQ, context bank IRQs
                                ↓
Current QEMU Status:      ABSENT (Peripherals use direct linear physical addressing)
Missing Functionality:    Device virtual address space isolation and IO page fault handling
```

---

## 2. Dependency Matrix Summary

| Subsystem | Userspace Contract | Kernel Driver | Qualcomm Hardware Device | QEMU Implementation | Blocker Risk |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **GPU / KGSL** | `/dev/kgsl-3d0`, ioctls | `drivers/gpu/msm/kgsl.c` | `qcom,adreno-612` (`0x05090000`) | Absent | **PRIMARY CANDIDATE 1** |
| **Display / DPU**| `/dev/dri/card0` | `drivers/gpu/drm/msm/` | `qcom,sde-kms` (`0x0ae00000`) | Replaced by `ramfb` | **PRIMARY CANDIDATE 2** |
| **VSYNC** | DRM vblank event | `dpu_encoder.c` | MDSS Timing Engine | Synthetic software tick | Secondary |
| **Gralloc / ION**| `/dev/ion`, `/dev/hwbinder` | `msm_ion.c` | ION Carveouts | DTB Carveout | Functional (software) |
| **SMMU** | DMA / IOMMU API | `arm-smmu.c` | APPS SMMU (`0x15000000`) | Direct physical mapping | Non-blocking currently |

---

## 3. Decision Gate for Device Selection

Before selecting either **KGSL / Adreno GPU** or **MDSS / DPU** as the first virtual device to build, **Phase 1.3 must empirically prove** which interface the guest is currently waiting on at the stall boundary.
