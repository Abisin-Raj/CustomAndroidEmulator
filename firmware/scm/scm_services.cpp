#include "firmware/scm_services.h"
#include "core/logger.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>

namespace firmware {

// ---------------------------------------------------------------------------
// QCOM SCM service command IDs
// ---------------------------------------------------------------------------
constexpr uint8_t CMD_BOOT_SET_ADDR    = 0x01; // Legacy boot address setting
constexpr uint8_t CMD_BOOT_SET_ADDR_MC = 0x11; // Multi-core boot address setting (0xC2000111 or similar)
constexpr uint8_t CMD_INFO_IS_CALL_AVAIL = 0x01; // Info availability query

// ---------------------------------------------------------------------------
// QcomBootService::handle
// ---------------------------------------------------------------------------
SmcResult QcomBootService::handle(const SmcRequest& req) {
    switch (req.id.command()) {

    case CMD_BOOT_SET_ADDR: {
        // args[0] (X1) is arginfo
        // args[1] (X2) is CPU flags
        // args[2] (X3) is physical entry point address
        m_warmBootAddr = req.args[2];
        spdlog::info("[SCM] QCOM_SCM_BOOT_SET_ADDR — boot addr registered: 0x{:016x} (returning SUCCESS)",
            m_warmBootAddr);
        return { SMCCC_RET_SUCCESS, req.args[0], req.args[1], req.args[2], true };
    }

    case CMD_BOOT_SET_ADDR_MC: {
        // args[0] (X1) is arginfo
        // args[1] (X2) is physical entry point address
        // args[2] (X3) to args[5] (X6) are affinity levels
        // args[6] (X7) is flags (passed inside req.args[5])
        m_warmBootAddr = req.args[1];
        spdlog::info("[SCM] QCOM_SCM_BOOT_SET_ADDR_MC — warm boot addr registered: 0x{:016x} (returning SUCCESS)",
            m_warmBootAddr);
        return { SMCCC_RET_SUCCESS, req.args[0], req.args[1], req.args[2], true };
    }

    default: {
        spdlog::warn("[SCM] UNIMPLEMENTED boot service command 0x{:02x} (service=0x01)",
            static_cast<int>(req.id.command()));
        return { SMCCC_RET_NOT_SUPPORTED, req.args[0], req.args[1], req.args[2], false };
    }

    } // switch
}

// ---------------------------------------------------------------------------
// QcomInfoService::handle
// ---------------------------------------------------------------------------
SmcResult QcomInfoService::handle(const SmcRequest& req) {
    switch (req.id.command()) {

    case CMD_INFO_IS_CALL_AVAIL: {
        // args[0] (X1) is arginfo
        // args[1] (X2) is the SCM Function ID being queried
        uint32_t queriedFn = static_cast<uint32_t>(req.args[1] & 0xFFFFFFFF);
        
        spdlog::info("[SCM] QCOM_SCM_INFO_IS_CALL_AVAIL — queried Fn ID: 0x{:08x}", queriedFn);

        // Return X0=1 (QCOM_SCM_V1_INTR / interrupted) so the guest follows the
        // resume path at a26c. This lets us observe 0x00000001 entry registers
        // and the caller's code around LR=a794 in the next boot trace.
        // X0=1, X1=1 (available), preserve X2/X3.
        SmcResult res;
        res.x0 = 1; // Triggers interrupted/resume path
        res.x1 = 1; // Available
        res.x2 = req.args[1]; // Preserve original X2
        res.x3 = req.args[2]; // Preserve original X3
        res.handled = true;
        return res;
    }

    default: {
        spdlog::warn("[SCM] UNIMPLEMENTED info service command 0x{:02x} (service=0x06)",
            static_cast<int>(req.id.command()));
        return { SMCCC_RET_NOT_SUPPORTED, req.args[0], req.args[1], req.args[2], false };
    }

    } // switch
}

} // namespace firmware
