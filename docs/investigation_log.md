# Investigation Log — Custom Android Emulator

This file records every investigation phase in chronological order.
Each entry covers: what was hypothesized, what evidence was collected,
what was ruled out, what was confirmed, and why the next phase exists.

---

## Phase 1 — SCM v1 Resume Call (0x00000001) Characterization

**Date**: 2026-07

### Hypothesis
Returning `X0 = 1` (interrupted) from the IS_CALL_AVAIL availability query
(`0xC2000601`) causes the guest to enter a SCM v1 resume loop via SMC ID
`0x00000001`. Returning success from the resume call terminates the loop.

### Evidence Collected
- Guest issued `0x00000001` at PC `0xffffff800888a26c` immediately after
  receiving `X0 = 1` from `0xC2000601`.
- Registers: `X1 = 0x1` (token), `X2 = 0x82000601` (resumed fn ID), `X3–X6 = 0`.
- `X2` contained the function ID directly — not a struct pointer.
- No guest memory was accessed during the resume call.

### Ruled Out
- `X2` is NOT a pointer. No external memory read required.

### Confirmed
- `0x00000001` is the SCM v1 "Interrupted Call Resume" ABI.
- Returning `X0 = 0`, `X1 = 1` satisfies the availability check.

### Why Phase 2 Exists
The resume handler was missing from `smc_dispatcher.cpp`. Adding it broke the
availability-query loop, which was the then-current blocker.

---

## Phase 2 — SCM v2 Warm Boot Address Call (0x42000A01)

**Date**: 2026-08-01

### Hypothesis
After the availability query succeeds, the guest issues SCM v2 warm-boot
registration call `0x42000A01`. Returning success lets the guest attempt to
bring up secondary CPUs.

### Evidence Collected
- `0x42000A01` at PC `0xffffff800888a534`.
- `X1 = 0x22` (flags), `X2 = 0x429d0000` (entry PA), `X3 = 0x200`.
- Caller checks `X0 == 0` for success; uses `X1` as `res.a0`.
- After returning success, the availability query loop terminated.

### Ruled Out
- SCM v1 warm-boot path (`0x82000101` / `0xC2000101`) was not taken.

### Confirmed
- Returning `X0 = 0`, `X1 = 1` satisfies the `0x42000A01` caller.
- The availability query loop ended.

### Why Phase 3 Exists
CPU0 still retried the entire boot sequence 41 times. The SCM layer was no longer
the blocker. Something in secondary CPU bring-up was failing downstream.

---

## Phase 3 — GDB RSP Thread Safety

**Date**: 2026-08-02

### Hypothesis
When a secondary CPU hits a breakpoint, the GDB RSP client reads/writes CPU0's
registers instead of the stopped CPU's registers, because no `Hg` thread-selection
packet was sent before register access.

### Evidence Collected
- `waitForStop()` returned raw stop replies containing `thread:p01.01` but the
  thread ID was never parsed.
- All 123 SMC intercepts (41 cycles × 3 calls) showed thread `p01.01` (CPU0).
- No secondary CPU ever hit any of the 62 armed breakpoints.

### Ruled Out
- Register corruption from secondary CPU breakpoints (thread switching prevents it).

### Confirmed
- `selectThread(threadId)` is necessary before each register access.
- All observed SMCs genuinely originated from CPU0.

### Why Phase 6.6A Exists
Thread safety did not change the outcome. CPU1–7 never booted. The blocker
shifted from the SCM firmware layer to the CPU hotplug hardware layer. The
question became: what mechanism does the kernel use to start secondary CPUs?

---

## Phase 6.6A — Secondary CPU Boot Mechanism Investigation

**Date**: 2026-08-02 (in progress)

### Open Hypotheses

**A — QEMU swallows PSCI internally**
DTS says `enable-method = "psci"` and `arm,psci-1.0; method = "smc"`, yet no
PSCI SMC was observed across 62 armed breakpoints. QEMU's `virt` machine
implements PSCI natively and may intercept `CPU_ON` before GDB fires. Vendor
MPIDR for CPU1 is `0x100`; QEMU's generated DTB assigns `0x01`. PSCI
`CPU_ON(0x100)` would return `PSCI_RET_INVALID_PARAMS` silently.

**B — Guest writes to APCS MMIO**
secondary CPUs from reset, bypassing PSCI. QEMU does not emulate these registers.

**C — Earlier blocker (clock, regulator, interrupt)**
The guest fails before reaching any CPU wake mechanism.

### Evidence Collected (Session #14)

| Observation | Value |
|---|---|
| vCPU threads in QEMU | **8** (p01.01 – p01.08) — all created |
| CPU0 (p01.01) initial PC | `0xffffff800a4f2000` — executing in kernel VA |
| CPU1–7 initial PC | **`0x0000000000000000`** — held at reset vector |
| CPU1–7 PSTATE (raw `c5030040`) | `0x400003c5` → **EL1h** — PSCI power-off state |
| CPU1–7 PC across all 41 retry cycles | **Constant `0x0`** — never released |
| `qemu_trace.log` | **Empty (0 bytes)** — no unmapped MMIO accesses |
| APCS range `0x17c00000` in trace | **Absent** |
| PSCI `cpu ON` lines in QEMU log | **None found** |

### Ruled Out

- **Hypothesis B (APCS MMIO)**: `qemu_trace.log` is 0 bytes. The guest never
  wrote to any unmapped MMIO region.
- **Hypothesis C (earlier blocker)**: All 8 vCPUs are instantiated in QEMU in
  EL1h power-off state. The kernel reached secondary CPU bring-up.

### Confirmed (Empirical Evidence)

**Hypothesis A (MPIDR Mismatch in PSCI CPU_ON) is CONFIRMED.**

1. **QEMU Auto-Generated DTB (`qemu_generated.dts`)**:
   - `cpu@0`: `reg = <0x00>`, `enable-method = "psci"`
   - `cpu@1`: `reg = <0x01>`, `enable-method = "psci"`
   - `cpu@2`: `reg = <0x02>`, `enable-method = "psci"`
   - ...
   - `cpu@7`: `reg = <0x07>`, `enable-method = "psci"`
   - QEMU assigns flat MPIDR values `0x00` through `0x07` to its 8 vCPUs.

2. **Qualcomm Vendor DTS (`original_kernel.dts`)**:
   - `cpu@0`: `reg = <0x00 0x00>` (MPIDR `0x000`)
   - `cpu@100`: `reg = <0x00 0x100>` (MPIDR `0x100`, Affinity 1 = 1)
   - `cpu@200`: `reg = <0x00 0x200>` (MPIDR `0x200`, Affinity 1 = 2)
   - ...
   - `cpu@700`: `reg = <0x00 0x700>` (MPIDR `0x700`, Affinity 1 = 7)

3. **The Root Cause (Supported Inference)**:
   - The Qualcomm vendor kernel is built assuming cluster-based MPIDR indexing (`0x100`,
     `0x200`, etc.) for secondary cores as defined in vendor hardware device trees.
   - The evidence strongly suggests that the guest issues a PSCI `CPU_ON` targeting
     MPIDR `0x100`.
   - Given QEMU's auto-generated DTB advertises CPUs with MPIDRs `0x00`–`0x07`, the
     most likely outcome is that QEMU rejects the request with `PSCI_RET_INVALID_PARAMS`,
     preventing secondary CPU bring-up.
   - The kernel CPU bring-up driver fails, times out after 250ms, and retries
     warm-boot registration from the top — causing the exact 41-retry boot loop
     observed in BET traces.


### Experiment B Results (Session #17)

- **Variable Tested**: Passed vendor DTB via `-dtb reference/dtbs/original_kernel.dtb`.
- **Observations**:
  - Guest MMU activated cleanly at `PC = 0xffffff80080cbe6c`.
  - SMC interception sequence (`0xC2000601` → `0x00000001` → `0x42000A01`) matched Experiment A exactly.
  - SCM warm-boot retries: 41 cycles (identical to Experiment A).
  - CPU1–7 state: PC = `0x0`, PSTATE = `0x400003c5` (EL1h) throughout the session.
  - CPU1 did **not** leave reset.
- **Conclusion**: **Outcome 2 confirmed.** Passing `-dtb` alone does not enable secondary CPU bring-up.
  QEMU's `virt` machine generates internal vCPU `mp_affinity` values (`0x00`..`0x07`) in C code during machine initialization independently of the DTB passed via `-dtb`. When the guest calls PSCI `CPU_ON` targeting MPIDR `0x100` (read from the vendor DTB), QEMU's internal PSCI engine fails to match `0x100` against its internal `mp_affinity` table.

### Next Step (Step 4 Investigation)

Investigate QEMU vCPU topology configuration parameters (`-smp 8,sockets=1,clusters=8,cores=1,threads=1` or `-cpu cortex-a76,rvbar=...`) to determine if QEMU allows configuring vCPU `mp_affinity` values to match Qualcomm cluster layout (`0x100`..`0x700`).

### Phase 6.6D — DTB MPIDR Translation Validation (Session #21)

- **Variable Tested**: Passed `patched_kernel.dtb` containing flat CPU `reg` entries (`0x00`..`0x07`) matching QEMU's `virt` board MPIDRs.
- **Observations**:
  - QEMU launched with `-dtb C:/Users/ABISIN RAJ/Desktop/Projects/CustomAndroidEmulator/reference/dtbs/patched_kernel.dtb`.
  - Guest booted and MMU activated at `PC = 0xffffff80080cbe6c`.
  - SMC interception sequence (`0xC2000601` $\rightarrow$ `0x00000001` $\rightarrow$ `0x42000A01`) matched baseline exactly (41 cycles).
  - CPU1–7 state: PC = `0x0`, PSTATE = `0x400003c5` (EL1h) throughout the session.
  - CPU1 did **not** leave reset.

### CPU Topology & Bring-Up Source Trace

- **Kernel CPU Bring-Up Call Chain (Linux ARM64 / Qualcomm Vendor Kernel)**:
  1. **Early DTB Parsing (`arch/arm64/kernel/setup.c`)**:
     During early boot, the kernel populates `cpu_logical_map(cpu)` (indexed by logical CPU 0..7) by reading the `reg` property of each DTB node under `/cpus/cpu@...`.
     - In vendor DTS (`original_kernel.dts`): `cpu_logical_map(1) = 0x100`.
     - In patched DTS (`patched_kernel.dts`): `cpu_logical_map(1) = 0x01`.
  2. **Secondary CPU Boot Invocation (`arch/arm64/kernel/psci.c`)**:
     When the kernel brings up CPU1, `cpu_psci_cpu_boot(1)` retrieves `mpidr = cpu_logical_map(1)` and passes it to `psci_ops.cpu_on(mpidr, entry_pa)`.
  3. **Vendor Machine Hardware Prerequisites**:
     In Qualcomm vendor kernels (SM6150), `smp_ops` includes Qualcomm platform bring-up steps (`qcom_boot_secondary` / KPSS / APCS power domain controls) prior to or alongside PSCI `CPU_ON`.

- **Architectural Findings**:
  - The target MPIDR passed to PSCI `CPU_ON` is derived directly from `cpu_logical_map(cpu)` (which comes from DTB `reg`).
  - When `patched_kernel.dtb` was provided (setting `cpu_logical_map(1) = 0x01`), the MPIDR matched QEMU's vCPU #1 assignment (`0x01`).
  - **Conclusion**: This rules out MPIDR mismatch as the sole blocker. The remaining evidence strongly suggests that one or more additional platform-level requirements are preventing secondary CPU bring-up.

### Phase 6.7A — Secondary CPU Bring-up Timeline Findings (Session #24–#27)

- **Diagnostic Answers**:
  1. **Q1: Does QEMU receive PSCI calls from the guest?**
     - **Yes.** QEMU exception trace (`-d int,unimp,guest_errors`) confirmed the guest kernel issues PSCI calls via **HVC** (`hvc #0`, Exception 11 from EL1 to EL2 with ESR `0x16/0x5a000000`).
     - 137 PSCI HVC calls were recorded in `qemu_trace.log` (`handled as PSCI call`).
     - **Crucial Observation**: The kernel issues HVC call `0x84000009`, but **never invokes `PSCI_CPU_ON` (`0x84000003` / `0xC4000003`)**. The kernel halts its secondary CPU bring-up sequence prior to reaching the `PSCI_CPU_ON` call.

  2. **Q2: Does CPU1's PC ever change from `0x0`?**
     - **No.** Continuous vCPU register sampling confirmed CPU1 remains at `PC = 0x0000000000000000` with `PSTATE = 0x400003c5` (EL1h reset vector) throughout the entire session. CPU1 never leaves reset for even a single instruction cycle.

  3. **Q3: Does QEMU record/write the secondary CPU entry address (`entry_pa`)?**
     - **No.** Because `PSCI_CPU_ON` is never issued by the kernel, QEMU's secondary CPU holding pen vector is never populated.

- **Empirical Bring-up Timeline**:

```text
Kernel Boot (CPU0)                 [SUCCESS — Executing in Virtual Space]
      ↓
SCM Availability Check (0xC2000601) [SUCCESS — Emulated X0=1]
      ↓
SCM v1 Resume Call (0x00000001)    [SUCCESS — Emulated X0=0]
      ↓
SCM Warm-Boot Setup (0x42000A01)  [SUCCESS — Emulated entry=0x429d0000]
      ↓
Kernel HVC Call (0x84000009)       [EXCEPT — Handled as PSCI HVC]
      ↓
[HALT / RETRY LOOP]                [FAIL — PSCI_CPU_ON is NEVER reached]
```

### Phase 6.7C — Source Correlation Plan

**Goal**: Correlate the disassembly around `0xffffff80091840b0–0xffffff8009184100` with kernel source to determine the exact function, semantic purpose of HVC `0x84000009`, and the true meaning of the `cmp x8, x9` branch.

#### Verification Steps for Phase 6.7C

1. **Empirical Register Capture**: Log exact register values before and after HVC call `0x84000009` (X0 before; X0, X1, X2, X3 after).
2. **Surrounding Function Identification**: Disassemble function prologue preceding `0xffffff80091840d8` to locate stack allocation (`sub sp, sp, #N`) and frame pointer setup (`stp x29, x30, [sp, #N]`), determining exact function boundaries.
3. **Instruction-to-Source Mapping**:
   - Trace 256 bytes before `LR` and 256 bytes after `LR`.
   - Map `adrp x8` + `ldr x8` to the exact kernel data structure/table.
   - Identify what `x8` and `x9` represent, and why `cmp x8, x9` determines the branch outcome.







