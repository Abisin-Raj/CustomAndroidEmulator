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

