# Qualcomm Secure Channel Manager (SCM) Call Analysis

This document catalogs and analyzes the Secure Monitor Calls (SMC) executed by the guest kernel to transition to EL3 secure firmware.

---

## Cataloged SMC Calls

### SMC ID: `0xC2000601`

| Field | Description |
|---|---|
| **SMC ID** | `0xC2000601` (SMC64 Fast Call, SIP Owner `0x2`, Service `0x06`, Command `0x01`) |
| **Common Name** | `QCOM_SCM_BOOT_ADDR_MC` (or `QCOM_SCM_SET_WARM_BOOT_ADDR`) |
| **Calling Function** | `qcom_scm_set_boot_addr_mc` |
| **Driver** | `drivers/firmware/qcom/scm-boot.c` (or `qcom_scm.c` depending on kernel tree configuration) |
| **Registers In** | `X0` = `0xC2000601`<br>`X1` = Physical entry point address (`virt_to_phys(entry)`)<br>`X2` = `~0ULL` (CPU affinity mask)<br>`X3` = `~0ULL`<br>`X4` = `~0ULL`<br>`X5` = `~0ULL`<br>`X6` = `flags` |
| **Expected Registers Out** | `X0` = `0` (Success) or SCM error code |
| **Mandatory** | **Yes** (fails CPU bring-up/idle-state initialization if return status is unhandled or returns error) |
| **Current Emulator Behavior** | **Without `secure=on`:** Raises an `Undefined Instruction` exception at EL1.<br>**With `secure=on`:** Routes SMC to EL3 where guest CPU branches to secure vector `0x200` and executes `udf #0` instructions in a loop. |

* **SCM Compatibility Strategy:**
  * **Stubbing / Emulation:** Intercept SMC instruction execution at CPU level and inject return value `X0 = 0` (Success).
  * **Kernel Patching:** Stub out the driver calls inside the kernel's scm driver binary or source.

---

## Phase 6.5B — Kernel Source Code Trace

### Execution Path to `QCOM_SCM_BOOT_ADDR_MC (0xC2000601)`

The following trace documents the complete call chain from kernel startup to the blocking SMC instruction, based on Qualcomm MSM8953 / SDM670 kernel source (`msm-4.19` / `android-msm-violet-4.14`).

#### 1. Entry Point — Kernel Startup

```
arch/arm64/kernel/head.S
  start_kernel()                     → init/main.c
    smp_prepare_boot_cpu()           → arch/arm64/kernel/smp.c
    smp_init()                       → kernel/smp.c
      cpu_up(cpu)
        _cpu_up(cpu, 0, CPUHP_ONLINE)
          __cpu_up(cpu, idle)        → arch/arm64/kernel/smp.c
            boot_secondary(cpu)
              cpu_ops[cpu]->cpu_boot()   → arch/arm64/kernel/cpu_ops.c
```

#### 2. CPU Operations — PSCI Boot Path

```
arch/arm64/kernel/cpu_ops.c
  cpu_boot() → psci_cpu_boot()       → drivers/firmware/psci.c
    psci_ops.cpu_on(cpu_logical_map(cpu), __pa_symbol(secondary_entry))
      → psci_cpu_on()
          invoke_psci_fn(PSCI_FN_NATIVE(1_0, CPU_ON), ...)
```

> At this point on MSM kernels, PSCI is redirected through `qcom_scm` rather than direct PSCI SMCs when the QCOM SCM firmware layer is present.

#### 3. Qualcomm SCM Boot Driver

```
drivers/firmware/qcom/scm-boot.c   (or drivers/soc/qcom/scm.c on older trees)

qcom_scm_set_warm_boot_addr(void *entry, const cpumask_t *cpus)
  → qcom_scm_set_boot_addr_mc(virt_to_phys(entry), affinity_mask, ~0, ~0, ~0, flags)
    → qcom_scm_call(dev, SCM_SVC_BOOT=0x01, SCM_BOOT_ADDR_MC=0x01, ...)
      → __qcom_scm_call_do_quirk()
        → qcom_scm_call_smccc()
          → arm_smccc_smc(0xC2000601, ...)   ← SMC INSTRUCTION EXECUTED
```

#### 4. SMC Instruction Encoding

```
X0 = 0xC2000601   (Fast Call, SMC64, SIP = 0x02, Service = 0x06, Command = 0x01)
X1 = physical address of secondary_entry (warm boot entrypoint)
X2 = CPU affinity mask (~0ULL = all CPUs)
X3-X5 = ~0ULL
X6 = flags (0 = default)
```

#### 5. Required Firmware Response

The kernel **only** checks that `X0 == 0` (QCOM_SCM_V2_EBUSY or any nonzero = error). The actual secondary CPU bring-up requires:
1. EL3 firmware records the physical address in `X1`.
2. Firmware acknowledges with `X0 = 0` (success return).
3. On subsequent CPU power-on events, firmware executes a branch to the recorded address.

For the emulator's **single-CPU** use-case (only CPU0 active), the secondary CPU warm boot address registration can be safely **stubbed** with `X0 = 0` without needing to actually record or execute the warm boot address.

#### 6. Symbol Resolution Confirmation

Using the `kernel_analysis::SymbolResolver` module, PC addresses from the live diagnostic run are resolved as follows:

| Raw PC Address | Resolved Symbol |
|---|---|
| `0xffffff800888a25c` | `qcom_scm_set_warm_boot_addr+0xc` |
| `0xffffff800888a794` | Return address (Link Register X30) — caller in warm boot chain |

#### 7. Minimum Required SCM Stub Behavior

```text
When: X0 == 0xC2000601 (QCOM_SCM_BOOT_ADDR_MC)

  Stub Action:
    Record argument in X1 (warm boot entry point) [optional for single-CPU]
    Set X0 = 0  (success)
    ERET back to EL1 caller

Failure to return 0:
  → Kernel logs "Failed to set warm boot address"
  → CPU bring-up continues attempting secondary boots
  → May loop indefinitely or panic on SMP barrier timeout
```

---

## Phase 6B Implementation Target — SCM Dispatcher Design

Based on this trace, the **first firmware compatibility service** to implement is the SMC dispatcher stub:

```
firmware/
  dispatcher/
    smc_dispatcher.cpp     ← Routes by (X0 & 0xFFFF0000) service ID
  scm/
    scm_stub.cpp           ← Handles 0xC2000601 → return X0=0
```

### Minimum Viable Dispatcher Logic (Phase 6B Step 1)

```cpp
// When SMC instruction is trapped at EL3:
// x0 = SMC Function Identifier
uint64_t dispatch(uint64_t x0, uint64_t x1, ...) {
    uint32_t svc_id = (x0 >> 8) & 0xFF;   // Bits [15:8]
    uint32_t cmd_id = (x0 >> 0) & 0xFF;   // Bits [7:0]

    if (svc_id == 0x06 && cmd_id == 0x01) {
        // QCOM_SCM_BOOT_ADDR_MC — register warm boot address
        // For single-CPU: acknowledge without action
        return 0;  // QCOM_SCM_RESULT_SUCCESS
    }

    // Unhandled call — log and return generic error
    return 0xFFFFFFFF;  // QCOM_SCM_ERROR_UNKNOWN
}
```

The next step (Phase 6B implementation) is to wire this dispatcher into QEMU's TCG accelerator through a custom machine type or `-plugin` hook so that `smc` instructions are trapped before the guest CPU branches to `0x200`.


