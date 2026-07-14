#pragma once
#include "firmware/iscm_service.h"

namespace firmware {

// ---------------------------------------------------------------------------
// QcomBootService — handles QCOM SIP Boot service calls (service_id = 0x06)
//
// Owner:   0x02 (SIP — Silicon-specific calls)
// Service: 0x06 (QCOM Boot Services)
//
// Implemented commands:
//   0x01 — QCOM_SCM_BOOT_ADDR_MC   (0xC2000601)
//           Registers a warm-boot entry point for secondary CPUs.
//           Required during SMP initialization. Returns X0 = 0 (success).
// ---------------------------------------------------------------------------
class QcomBootService final : public IScmService {
public:
    QcomBootService() = default;
    ~QcomBootService() override = default;

    uint8_t owner()   const override { return 0x02; } // SIP owner
    uint8_t service() const override { return 0x06; } // Boot service group

    SmcResult handle(const SmcRequest& req) override;

private:
    // Records the last registered warm boot address (x1 argument)
    uint64_t m_warmBootAddr = 0;
};

} // namespace firmware
