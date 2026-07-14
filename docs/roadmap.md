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
    - Slot suffix commandline mapping.
- [ ] **Phase 7: Virtual Device Framework**
    - Abstract devices: serial, timers, battery, and RTC clocks setups.
- [ ] **Phase 8: Device Tree & Boot Configuration**
    - DTB/DTBO device tree overlays parsing and merger.
- [ ] **Phase 9: Android Userspace Initialization**
    - Root filesystem mounting and SELinux checks bypasses.
- [ ] **Phase 10: Graphics & Input**
    - Framebuffer setups, SDL UI windows, and virtual touchscreens.
- [ ] **Phase 11: ADB, Debugging & Developer Tools**
    - ADB bridge connectivity and remote debugging consoles.
- [ ] **Phase 12: Performance & ROM Compatibility**
    - WHPX/KVM virtualization acceleration configurations.
