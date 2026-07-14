# Architectural Compatibility Decisions

This document registers and explains the compatibility decisions, alternatives considered, and rationale behind workarounds for emulator-to-hardware deviations.

---

## Decision Log

### DECISION-001: Qualcomm SCM Fast Call Bypassing (`0xC2000601`)
* **Problem:** During early boot, the guest kernel executes a Secure Monitor Call (`smc #0`) with ID `0xC2000601` (`QCOM_SCM_SET_WARM_BOOT_ADDR`) to register secondary CPU boot entry points. Without EL3 support, QEMU raises an Undefined Instruction exception. With EL3 enabled, execution branches to unmapped secure exception vector space and loops on `udf #0`.
* **Evidence:** Traced via QEMU register monitor and disassembler logs in `diagnostics/2026-07-14/`.
* **Alternatives Considered:**
  1. **EL3 Firmware Simulation (e.g. ATF/QTEE):** Build or load a minimal Arm Trusted Firmware binary to run at EL3. *Status: Rejected due to high implementation complexity and lack of Qualcomm-specific security services documentation.*
  2. **Intercepting SMC in QEMU:** Modify QEMU simulation loop to hook the `smc` instruction and emulate register changes. *Status: Under evaluation.*
  3. **Kernel Source Patching:** Disable SCM boot address setting in the kernel source or stub the calling function (`qcom_scm_set_boot_addr_mc`). *Status: Recommended for early prototyping if kernel code allows.*
* **Chosen Solution:** *Pending research on kernel source code tracing (Phase 6.5B).*
