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

    // Handle the special SCM v1 resume call (0x00000001)
    if (req.id.raw == 0x00000001ULL) {
        if (x2 == 0x82000601ULL || x2 == 0xC2000601ULL) {
            spdlog::info("[SCM] Handling SCM v1 resume (0x00000001) for availability query 0x{:08x}", x2);
            SmcResult res;
            res.x0 = SMCCC_RET_SUCCESS; // 0 (success)
            res.x1 = 1;                 // 1 (available)
            res.x2 = x2;                // Preserve target command
            res.x3 = x3;                // Preserve original X3
            res.handled = true;
            return res;
        } else {
            spdlog::warn("[SCM] Unhandled target command 0x{:08x} on resume call (0x00000001)", x2);
            return { SMCCC_RET_NOT_SUPPORTED, x1, x2, x3, false };
        }
    }

    // Walk registered services and find a matching (owner, service) pair
    for (auto& svc : m_services) {
        if (svc->owner()   == req.id.owner()   &&
            svc->service() == req.id.service()) {
            SmcResult result = svc->handle(req);
            if (result.handled) {
                spdlog::debug("[SCM] Handled — Cmd=0x{:02x} → X0=0x{:x}",
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
