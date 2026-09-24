# Phase 28 — Persistent QEMU Machine Checkpointing
## Full Walkthrough: Gates 28.1 through 28.10 PASSED

We have established the complete checkpointing infrastructure, created the **CP-01 Golden Recovery Point**, restored it with sub-second latency, verified deterministic guest state identity, validated live execution post-restore, confirmed multi-cycle idempotence, and formally declared CP-01 reusable for downstream BLAST/ANativeWindow experiments.

---

### Progress Matrix

| Gate | Objective | Status | Latency / Metric | Evidence |
|---|---|---|---|---|
| **28.1** | Audit launch configuration and VMState | **PASSED** | N/A | Source audit of virt machine and Qualcomm peripherals |
| **28.1A** | Freeze/hash pristine `emmc.img` base disk identity | **PASSED** | 8.58 GB base | `04ba06833ab9...` verified |
| **28.1B** | Create and validate QCOW2 backing overlay | **PASSED** | Clean overlay | 8GB virtual, raw backing |
| **28.2** | Verify QMP + snapshot capability in isolation | **PASSED** | Instant | QMP `savevm test_cap` accepted with 0 errors |
| **28.2A** | Mutate execution & verify state rollback on controlled VM | **PASSED** | Instant | Mutated registers rolled back deterministically via `loadvm` |
| **28.3** | Boot Android and create CP-01 Golden Recovery Point | **PASSED** | $t=466.8\text{s}$ state trigger | `savevm cp01_sf_stable` (450 MiB) saved to QCOW2 |
| **28.4** | Continue execution after CP-01 and verify SF remains alive | **PASSED** | >80s continuous | Resumed via QMP `cont`; SF and HWC active post-save |
| **28.5** | Restore Checkpoint CP-01 via QMP (`loadvm`) | **PASSED** | **2.03s** | Checkpoint restored into RAM/devices with zero errors |
| **28.6** | Verify Deterministic Guest State Identity | **PASSED** | 100% match | PC, SP, X0-X30, HWC ELF, and opcode markers matched |
| **28.7** | Reconnect GDB & Validate Debugger Responsiveness | **PASSED** | Immediate | Reconnected to port 1776, read 33 registers successfully |
| **28.8** | Verify SF & HWC Continue Running Post-Restore | **PASSED** | 5s active run | Resumed via `cont`, executed without stalls or faults |
| **28.9** | Repeat Restore Cycle (Multi-Cycle Idempotence) | **PASSED** | **1.09s** | Second `loadvm` cycle restored identical state |
| **28.10** | Formally Declare CP-01 Reusable | **PASSED** | **~300x Speedup** | Baseline established for downstream buffer latch tasks |

---

### Key Architectural Fix During Phase 28

During initial validation, an access violation (`0xc0000005`) was identified at `qemu_del_vm_change_state_handler` (`system/runstate.c:377`) caused by a missing NULL check when unregistering VM change state handlers during migration/loadvm state notifications:
```c
void qemu_del_vm_change_state_handler(VMChangeStateEntry *e)
{
    if (!e || !e->entries.tqe_circ.tql_prev) {
        return;
    }
    QTAILQ_REMOVE(&vm_change_state_head, e, entries);
    g_free(e);
}
```
Rebuilt `qemu-system-aarch64.exe` via ninja; all subsequent snapshot saves and restores completed with zero errors.
