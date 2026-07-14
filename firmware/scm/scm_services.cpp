#include "firmware/scm_services.h"
#include "core/logger.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>

namespace firmware {

// ---------------------------------------------------------------------------
// QCOM SCM Boot service command IDs (service = 0x06)
// ---------------------------------------------------------------------------
constexpr uint8_t CMD_BOOT_ADDR_MC = 0x01; // QCOM_SCM_BOOT_ADDR_MC (0xC2000601)

// ---------------------------------------------------------------------------
// QcomBootService::handle
//
// Dispatches by command ID within the boot service group (0x06).
// Only CMD_BOOT_ADDR_MC is currently implemented. All other commands in this
// service group are logged as WARN and returned as NOT_SUPPORTED.
// ---------------------------------------------------------------------------
SmcResult QcomBootService::handle(const SmcRequest& req) {
    switch (req.id.command()) {

    // -----------------------------------------------------------------------
    // 0xC2000601 — QCOM_SCM_BOOT_ADDR_MC
    //
    // Called by:  qcom_scm_set_warm_boot_addr()
    //             drivers/firmware/qcom/scm-boot.c
    //
    // Arguments:
    //   args[0] (X1) = Physical address of secondary CPU warm-boot entrypoint
    //   args[1] (X2) = CPU affinity mask (~0ULL = all CPUs)
    //   args[2-4]    = ~0ULL (unused)
    //   args[5] (X6) = flags
    //
    // Return:
    //   X0 = 0 (SMCCC_RET_SUCCESS) — kernel checks only this value
    //
    // Emulation strategy (single-CPU):
    //   Record the address for diagnostic logging only. No secondary CPUs
    //   will be powered on in the emulator, so the address does not need to
    //   be acted upon. Returning 0 unblocks the kernel's SMP init path.
    // -----------------------------------------------------------------------
    case CMD_BOOT_ADDR_MC: {
        m_warmBootAddr = req.args[0]; // X1 = physical entry point

        spdlog::info("[SCM] QCOM_SCM_BOOT_ADDR_MC — warm boot addr registered: 0x{:016x} (returning SUCCESS)",
            m_warmBootAddr);

        return { SMCCC_RET_SUCCESS, true };
    }

    // -----------------------------------------------------------------------
    // All other commands in service 0x06 are not yet implemented.
    // Log them so they can be triaged and added incrementally.
    // -----------------------------------------------------------------------
    default: {
        spdlog::warn("[SCM] UNIMPLEMENTED boot service command 0x{:02x} (service=0x06)",
            static_cast<int>(req.id.command()));

        // handled = false so the dispatcher logs the full NO HANDLER path too
        return { SMCCC_RET_NOT_SUPPORTED, false };
    }

    } // switch
}

} // namespace firmware
