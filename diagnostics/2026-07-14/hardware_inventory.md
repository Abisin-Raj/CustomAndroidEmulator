# Hardware Inventory: Phone vs. QEMU

This document registers the compatibility status and mappings of physical hardware configurations on the Xiaomi Redmi Note 7 Pro (violet) against the virtual QEMU AArch64 `virt` board.

---

## 1. Compatible Components
* **Interrupt Controller (GIC):**
  * Target: GICv3 (`arm,gic-v3`)
  * QEMU: GICv3 (`arm,gic-v3`)
  * Status: ✓ Fully compatible; interrupt mapping structure is identical.
* **System Timer:**
  * Target: ARM Generic Architected Timer
  * QEMU: ARM Generic Architected Timer
  * Status: ✓ Fully compatible; registers map directly.
* **Power State Interface (PSCI):**
  * Target: PSCI v1.0 (via `hvc` method)
  * QEMU: PSCI v1.0 (via `hvc` method)
  * Status: ✓ Fully compatible.

## 2. Different Components
* **CPU Topology & Core Affinity:**
  * Target: Kryo 460 (2x Cortex-A76 + 6x Cortex-A55)
  * QEMU: Cortex-A53 / Cortex-A57 / max
  * Status: • Different; core layouts and capacity scaling vary, but core instruction sets are fully ARMv8-A compatible.
* **DRAM Physical Base Address:**
  * Target: DRAM starts at `0x80000000` (2 GB mark)
  * QEMU: DRAM starts at `0x40000000` (1 GB mark)
  * Status: • Different; absolute memory reservation targets (e.g. `0xa1600000` for ramoops) assume the target base memory map.
* **Primary Serial Console (UART):**
  * Target: Qualcomm MSM GENI UART (`0x880000`)
  * QEMU: ARM PL011 PrimeCell UART (`0x9000000`)
  * Status: • Different; target kernel disables PL011. Logs are directed to ramoops persistent memory instead.
* **PCIe Host Bridge:**
  * Target: Qualcomm PCIe Controller
  * QEMU: Generic PCI Host Bridge (ECAM)
  * Status: • Different.

## 3. Missing Components
* **Primary Storage Controller:**
  * Target: Qualcomm SDHCI controller (`sdhci@7c4000`)
  * QEMU: None (VirtIO block device driver is absent from target kernel)
  * Status: ✗ Missing; blocks mounting root (`/`) and `/vendor` partitions.
* **Secondary Storage Controller:**
  * Target: Qualcomm SDHCI controller (`sdhci@8804000`)
  * QEMU: None
  * Status: ✗ Missing; blocks SD card interface.
* **Qualcomm Secure Channel Manager (SCM):**
  * Target: EL3 Secure Monitor firmware handling SMC calls (`0xC2000601` etc.)
  * QEMU: None
  * Status: ✗ Missing; traps early boot inside an Undefined Instruction loop.
* **RPMh (Resource Power Manager):**
  * Target: Hardware power controllers
  * QEMU: None
  * Status: ✗ Missing.
* **TLMM (Top-Level Mode Multiplexer):**
  * Target: Qualcomm pin controller
  * QEMU: None
  * Status: ✗ Missing.

---

## 4. Platform Compatibility Confidence Score

This scorecard tracks the maturity and confidence of each emulation subsystem:

| Subsystem | Status | Confidence | Notes |
|---|---|---|---|
| **PSCI** | Compatible | 100% | Handled via generic QEMU PSCI interfaces. |
| **GICv3** | Compatible | 100% | Matches target interrupt controller configuration. |
| **Generic Timer** | Compatible | 100% | Architected timer registers match direct hardware. |
| **UART** | Workaround | 80% | Redirected from GENI to PL011 using commandline patches. |
| **Storage** | Unsupported | 0% | Qualcomm SDHCI controller is missing. |
| **SCM** | Investigation | 25% | Secure monitor calls are trapped; dispatcher layer planned. |

