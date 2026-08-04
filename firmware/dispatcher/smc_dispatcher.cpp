#include "firmware/smc_dispatcher.h"
#include "core/logger.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>

namespace firmware {

// ---------------------------------------------------------------------------
// SmcDispatcher::registerService
// ---------------------------------------------------------------------------
void SmcDispatcher::registerService(std::unique_ptr<IScmService> service) {
    if (!service) return;

    spdlog::info("[SCM] Registered service — Owner=0x{:02x} Service=0x{:02x}",
        static_cast<int>(service->owner()),
        static_cast<int>(service->service())
    );

    m_services.push_back(std::move(service));
}

// ---------------------------------------------------------------------------
// SmcDispatcher::dispatch
//
// 1. Build SmcRequest from register values
// 2. Find a registered service that matches (owner, service_id)
// 3. Forward to service's handle() method
// 4. If unhandled, return SMCCC_RET_NOT_SUPPORTED and log the full SMC ID
// ---------------------------------------------------------------------------
SmcResult SmcDispatcher::dispatch(uint64_t x0,
                                  uint64_t x1, uint64_t x2,
                                  uint64_t x3, uint64_t x4,
                                  uint64_t x5, uint64_t x6) {
    SmcRequest req;
    req.id.raw = static_cast<uint32_t>(x0 & 0xFFFFFFFF);
    req.args[0] = x1;
    req.args[1] = x2;
    req.args[2] = x3;
    req.args[3] = x4;
    req.args[4] = x5;
    req.args[5] = x6;

    spdlog::debug("[SCM] Received {}", req.id.toString());

    // Handle the SCM v1 / status poll call (0x00000001)
    if (req.id.raw == 0x00000001ULL) {
        spdlog::info("[SCM] Handling SCM poll/status call 0x00000001: X1=0x{:x} X2=0x{:x}", x1, x2);
        SmcResult res;
        res.x0 = 0; // 0 = Ready/Done
        res.x1 = 0;
        res.x2 = x2;
        res.x3 = x3;
        res.handled = true;
        return res;
    }

    // Handle SCM v1 boot/warm-boot SCM calls (0x82000101 / 0xC2000101)
    if (req.id.raw == 0x82000101ULL || req.id.raw == 0xC2000101ULL) {
        spdlog::info("[SCM] Handling SCM v1 warm-boot call 0x{:08x}", req.id.raw);
        SmcResult res;
        res.x0 = SMCCC_RET_SUCCESS; // 0 (success)
        res.x1 = 1;                 // 1 (success status for res.a0)
        res.x2 = x2;
        res.x3 = x3;
        res.handled = true;
        return res;
    }

    // Handle SCM v2 boot/warm-boot setting call (0x42000A01)
    // qcom_boot_secondary() uses: cbnz w0, error — so W0=0 means success.
    if (req.id.raw == 0x42000A01ULL) {
        spdlog::info("[SCM] Handling SCM v2 warm-boot setting call 0x42000A01: flags=0x{:x} entry=0x{:x} len=0x{:x}", x1, x2, x3);
        SmcResult res;
        res.x0 = 0;   // 0 = SUCCESS (cbnz w0 passes when W0==0)
        res.x1 = 0;
        res.x2 = 0;
        res.x3 = 0;
        res.handled = true;
        return res;
    }

    // -----------------------------------------------------------------------
    // PSCI basic handlers — QEMU virt with psci=on handles these natively,
    // but our passthrough path currently returns -1 before QEMU processes them.
    // We handle them here to let psci_probe() succeed.
    // -----------------------------------------------------------------------

    // SMCCC_VERSION (0x80000000): return SMCCC 1.1
    if (req.id.raw == 0x80000000ULL) {
        spdlog::info("[SMCCC] SMCCC_VERSION: returning 0x00010001 (SMCCC 1.1)");
        SmcResult res;
        res.x0 = 0x00010001;  // SMCCC 1.1
        res.x1 = 0; res.x2 = 0; res.x3 = 0;
        res.handled = true;
        return res;
    }

    // PSCI_VERSION (0x84000000): return PSCI 1.1
    if (req.id.raw == 0x84000000ULL) {
        spdlog::info("[PSCI] PSCI_VERSION: returning 0x00010001 (PSCI 1.1)");
        SmcResult res;
        res.x0 = 0x00010001;  // PSCI 1.1
        res.x1 = 0; res.x2 = 0; res.x3 = 0;
        res.handled = true;
        return res;
    }

    // PSCI_CPU_ON (0xC4000003 / 0x84000003): Return SUCCESS (0)
    if (req.id.raw == 0xC4000003ULL || req.id.raw == 0x84000003ULL) {
        spdlog::info("[PSCI Service] PSCI_CPU_ON: target_mpidr=0x{:x} entry=0x{:016x} context_id=0x{:x} -> returning SUCCESS (0)",
                     x1, x2, x3);
        SmcResult res;
        res.x0 = 0; // PSCI_RET_SUCCESS
        res.x1 = 0; res.x2 = 0; res.x3 = 0;
        res.handled = true;
        return res;
    }

    // qcom_scm_set_warm_boot_addr (0x42000A01 / 0x02000A01): Return SUCCESS (0)
    if (req.id.raw == 0x42000A01ULL || req.id.raw == 0x02000A01ULL) {
        spdlog::info("[SCM] qcom_scm_set_warm_boot_addr: entry=0x{:016x} -> returning SUCCESS (0)", x1);
        SmcResult res;
        res.x0 = 0; // SUCCESS
        res.x1 = 0; res.x2 = 0; res.x3 = 0;
        res.handled = true;
        return res;
    }

    // Qualcomm SiP Calls / SCM Hotplug & Power management (Owner 0x04 / 0x84000009)
    if (req.id.owner() == 0x04 || req.id.raw == 0x84000009ULL) {
        spdlog::info("[SCM] Handling Qualcomm SiP SMC call 0x{:08x}: X1=0x{:x} X2=0x{:x} -> returning SUCCESS (0)",
                     req.id.raw, x1, x2);
        SmcResult res;
        res.x0 = 0; // SUCCESS
        res.x1 = 0; res.x2 = 0; res.x3 = 0;
        res.handled = true;
        return res;
    }

    // PSCI_FEATURES (0x8400000A): feature probe
    // X1 holds the function ID being probed. Return 0 for known IDs, NOT_SUPPORTED (-2) otherwise.
    if (req.id.raw == 0x8400000AULL) {
        uint32_t probeId = static_cast<uint32_t>(x1 & 0xFFFFFFFF);
        bool supported = (probeId == 0x84000000 ||   // PSCI_VERSION
                          probeId == 0x80000000 ||   // SMCCC_VERSION
                          probeId == 0xC4000003 ||   // PSCI_CPU_ON (64-bit)
                          probeId == 0x84000003 ||   // PSCI_CPU_ON (32-bit)
                          probeId == 0x84000002 ||   // PSCI_CPU_OFF
                          probeId == 0xC4000001 ||   // PSCI_CPU_SUSPEND (64-bit)
                          probeId == 0x84000009 ||   // PSCI_SYSTEM_RESET
                          probeId == 0x8400000E ||   // PSCI_SYSTEM_RESET2 (32-bit)
                          probeId == 0xC400000E ||   // PSCI_SYSTEM_RESET2 (64-bit)
                          probeId == 0xC400000A ||   // PSCI_FEATURES
                          (probeId & 0x7FFFFFFF) != 0); // Default support for feature queries
        spdlog::info("[PSCI] PSCI_FEATURES probe for 0x{:08x}: {}",
                     probeId, supported ? "SUPPORTED(0)" : "NOT_SUPPORTED(-2)");
        SmcResult res;
        res.x0 = supported ? 0ULL : static_cast<uint64_t>(-2LL);
        res.x1 = 0; res.x2 = 0; res.x3 = 0;
        res.handled = true;
        return res;
    }

    // PSCI_CPU_SUSPEND (0x84000006 / 0xC4000006):
    // Return NOT_SUPPORTED — safe for SMP bring-up; kernel continues without WFI-based suspend.
    if (req.id.raw == 0x84000006ULL || req.id.raw == 0xC4000006ULL) {
        spdlog::info("[PSCI] PSCI_CPU_SUSPEND (0x{:08x}): returning NOT_SUPPORTED", req.id.raw);
        SmcResult res;
        res.x0 = static_cast<uint64_t>(-2LL);   // PSCI_RET_NOT_SUPPORTED
        res.x1 = 0; res.x2 = 0; res.x3 = 0;
        res.handled = true;
        return res;
    }

    // Walk registered services and find a matching (owner, service) pair
    for (auto& svc : m_services) {
        if (svc->owner()   == req.id.owner()   &&
            svc->service() == req.id.service()) {
            SmcResult result = svc->handle(req);
            if (result.handled) {
                spdlog::debug("[SCM] Handled — Cmd=0x{:02x} -> X0=0x{:x}",
                    static_cast<int>(req.id.command()), result.x0);
                return result;
            }
            // Service recognized the owner+service but not this command
            spdlog::warn("[SCM] UNIMPLEMENTED command — {}", req.id.toString());
            return { SMCCC_RET_NOT_SUPPORTED, x1, x2, x3, false };
        }
    }

    // No service handles this (owner, service_id) pair at all
    spdlog::warn("[SCM] NO HANDLER for {} — returning NOT_SUPPORTED", req.id.toString());
    return { SMCCC_RET_NOT_SUPPORTED, x1, x2, x3, false };
}

} // namespace firmware
