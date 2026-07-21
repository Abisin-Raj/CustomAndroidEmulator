#pragma once
#include "firmware/iscm_service.h"

namespace firmware {

// ---------------------------------------------------------------------------
// QcomBootService — handles QCOM SIP Boot service calls (service_id = 0x01)
//
// Owner:   0x02 (SIP — Silicon-specific calls)
// Service: 0x01 (QCOM Boot Services)
//
// Implemented commands:
//   0x01 — QCOM_SCM_BOOT_SET_ADDR  (legacy boot addr setting)
//   0x11 — QCOM_SCM_BOOT_SET_ADDR_MC
// ---------------------------------------------------------------------------
class QcomBootService final : public IScmService {
public:
    QcomBootService() = default;
    ~QcomBootService() override = default;

    uint8_t owner()   const override { return 0x02; } // SIP owner
    uint8_t service() const override { return 0x01; } // Boot service group

    SmcResult handle(const SmcRequest& req) override;

private:
    uint64_t m_warmBootAddr = 0;
};

// ---------------------------------------------------------------------------
// QcomInfoService — handles QCOM SIP Info service calls (service_id = 0x06)
//
// Owner:   0x02 (SIP — Silicon-specific calls)
// Service: 0x06 (QCOM Info Services)
//
// Implemented commands:
//   0x01 — QCOM_SCM_INFO_IS_CALL_AVAIL
// ---------------------------------------------------------------------------
class QcomInfoService final : public IScmService {
public:
    QcomInfoService() = default;
    ~QcomInfoService() override = default;

    uint8_t owner()   const override { return 0x02; } // SIP owner
    uint8_t service() const override { return 0x06; } // Info service group

    SmcResult handle(const SmcRequest& req) override;
};

} // namespace firmware
