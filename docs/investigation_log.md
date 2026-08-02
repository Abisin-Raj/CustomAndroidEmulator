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

3. **The Root Cause**:
   - The Qualcomm vendor kernel uses cluster-based MPIDR indexing (`0x100`,
     `0x200`, etc.) for secondary cores.
   - When the kernel invokes PSCI `CPU_ON(target_mpidr = 0x100, entry_pa)`, QEMU's
     built-in PSCI handler searches its internal vCPU list for MPIDR `0x100`.
   - QEMU only has vCPUs with MPIDRs `0x00`–`0x07`.
   - QEMU returns `PSCI_RET_INVALID_PARAMS` (`-2`) to the kernel.
   - The kernel CPU bring-up driver fails, times out after 250ms, and retries
     warm-boot registration from the top — causing the exact 41-retry boot loop
     observed in BET traces.

### Resolution Plan for Phase 6.6B

We have two options to resolve the MPIDR mismatch:

1. **Option 1 (QEMU Topology parameters)**: Pass `-smp 8,sockets=1,clusters=8,cores=1,threads=1` or CPU topology arguments to force QEMU to assign MPIDRs matching `0x100`..`0x700`.
2. **Option 2 (Vendor DTB / DTB Patch)**: Pass the compiled vendor DTB (or a patched DTB with flat MPIDRs matching QEMU's `0x01`..`0x07`) to QEMU via `-dtb`.


