# Emulator Roadmap

This document outlines the development roadmap of the CustomAndroidEmulator project.

---

## Roadmap Phases

- [x] **Phase 1: Foundation**
    - Modular CMake build configs setup.
    - spdlog Logger integration.
- [x] **Phase 2: Image Parsing**
    - Binary parsing of boot.img offsets.
    - ext4 filesystem signatures extraction.
- [x] **Phase 3: Boot Preparation**
    - Verification checklists.
    - boot_config.json configurations descriptor.
- [x] **Phase 4: VM Runtime**
    - Non-blocking QProcess execution.
    - Settings path detection.
- [x] **Phase 5: Android Boot Pipeline**
    - Decoupled IBootDetector structures.
    - EventDispatcher signals notifications.
    - BootSession stats logger.
- [ ] **Phase 6: Hardware Compatibility Layer**
    - Bridging Qualcomm registers to VirtIO block devices.
    - DTB/DTBO device tree overlays merger.
    - Slot suffix commandline mapping.
- [ ] **Phase 7: Virtual Device Framework**
    - Framebuffer/graphics setup.
    - Virtual keyboard/touchscreen devices.
- [ ] **Phase 8: Graphics & Display**
    - SDL displays rendering window.
    - GPU acceleration.
- [ ] **Phase 9: ADB & Debugging**
    - ADB bridge connectivity.
    - Remote console shells.
- [ ] **Phase 10: Android Userspace**
    - Mounting raw system/vendor partitions.
    - Handling Android init scripts.
- [ ] **Phase 11: ROM Compatibility**
    - GKI (Generic Kernel Image) support.
    - Dynamic partition slots.
- [ ] **Phase 12: Performance & Optimization**
    - Virtual virtualization flags (KVM/WHPX).
    - Timing benchmarks.
