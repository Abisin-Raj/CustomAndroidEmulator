# Device Tree (DTB) Comparison Report

This report catalogs and compares device tree nodes from the target Qualcomm Snapdragon 675 platform (`kernel_extracted.dts`) and the QEMU `virt` machine type (`virt.dts`).

---

## 1. Compatible Nodes

### PSCI (Power State Coordination Interface)
* **Phone:** `"arm,psci-1.0"`, `"arm,psci-0.2"`, `"arm,psci"` using the `hvc` method.
* **QEMU:** `"arm,psci-1.0"`, `"arm,psci-0.2"`, `"arm,psci"` using the `hvc` method.
* **Status:** ✓ **Compatible**.

### Interrupt Controller (GIC)
* **Phone:** `"arm,gic-v3"` at `/soc/intc`.
* **QEMU:** `"arm,gic-v3"` at `/intc`.
* **Status:** ✓ **Compatible**.

### Timers
* **Phone:** `"arm,armv8-timer"`
* **QEMU:** `"arm,armv7-timer"` (configured for AArch64 compatible timer offsets)
* **Status:** ✓ **Compatible**.

---

## 2. Different Nodes

### CPU Topology
* **Phone:** 8 cores (`cpu@0` through `cpu@7`), containing explicit capacities, power coefficients, and scheduling cost parameters.
* **QEMU:** Dynamically generated cores based on SMP parameters (configured via CPU architecture models).
* **Status:** • **Different** (topological layout differ, but instruction sets are compatible).

### DRAM Memory Layout
* **Phone:** Memory starts at base `0x80000000`.
* **QEMU:** Memory starts at base `0x40000000`.
* **Status:** • **Different**.

### UART / Serial Console
* **Phone:** Qualcomm MSM GENI UART at `/soc/qcom,qup_uart@0x880000`.
* **QEMU:** ARM PL011 PrimeCell UART at `/pl011@9000000`.
* **Status:** • **Different** (kernel expects GENI console driver; earlycon requires PL011 redirection).

---

## 3. Missing Nodes

### Qualcomm SDHCI (Storage)
* **Phone:** Primary root storage host at `/soc/sdhci@7c4000` (`"qcom,sdhci-msm-v5"`).
* **QEMU:** No matching hardware node (uses VirtIO block device profiles which the target kernel lacks).
* **Status:** ✗ **Missing**.

### Reserved Memory
* **Phone:** Extensive reserved blocks (`smem@86000000`, `hyp_region@85700000`, and `ramoops@a1600000` console buffer).
* **QEMU:** None by default.
* **Status:** ✗ **Missing**.

### Secure Channel Manager (SCM/SMC)
* **Phone:** Secure firmware gateway node (`/firmware/scm` or similar) executing secure monitor interfaces.
* **QEMU:** None.
* **Status:** ✗ **Missing** (directly triggers early EL1/EL3 crash states).

### RPMh & Power Domains
* **Phone:** Resource Power Manager hardware nodes for system power domains, clocks, and regulator control.
* **QEMU:** None.
* **Status:** ✗ **Missing**.

### Watchdog
* **Phone:** Qualcomm MSM watchdog timer at `0x17c10000`.
* **QEMU:** None (defaults to standard SP805 watchdog models).
* **Status:** ✗ **Missing**.

### SMEM (Shared Memory)
* **Phone:** Shared memory controller mapping IPC contexts between application and remote processor cores.
* **QEMU:** None.
* **Status:** ✗ **Missing**.
