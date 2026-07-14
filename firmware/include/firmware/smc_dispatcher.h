#pragma once
#include "firmware/smc_types.h"
#include "firmware/iscm_service.h"
#include <vector>
#include <memory>

namespace firmware {

// ---------------------------------------------------------------------------
// SmcDispatcher — central SMC routing table
//
// Usage:
//   SmcDispatcher dispatcher;
//   dispatcher.registerService(std::make_unique<QcomBootService>());
//   SmcResult result = dispatcher.dispatch(x0, x1, x2, x3, x4, x5, x6);
//
// Architecture:
//   1. Decodes SmcId from x0 register value
//   2. Searches registered services by (owner, service_id)
//   3. Delegates to the matched service's handle()
//   4. If no service matches, logs and returns SMCCC_RET_NOT_SUPPORTED
// ---------------------------------------------------------------------------
class SmcDispatcher {
public:
    SmcDispatcher() = default;
    ~SmcDispatcher() = default;

    // Register a service handler. Services are matched by (owner, service_id).
    void registerService(std::unique_ptr<IScmService> service);

    // Dispatch an SMC call using raw register values (x0-x6).
    // x0 encodes the SmcId; x1-x6 are service arguments.
    SmcResult dispatch(uint64_t x0,
                       uint64_t x1 = 0, uint64_t x2 = 0,
                       uint64_t x3 = 0, uint64_t x4 = 0,
                       uint64_t x5 = 0, uint64_t x6 = 0);

    size_t serviceCount() const { return m_services.size(); }

private:
    std::vector<std::unique_ptr<IScmService>> m_services;
};

} // namespace firmware
