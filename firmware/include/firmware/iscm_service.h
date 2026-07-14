#pragma once
#include "firmware/smc_types.h"

namespace firmware {

// ---------------------------------------------------------------------------
// IScmService — interface every SCM service module must implement
//
// A service handles a specific (owner, service_id) pair. The dispatcher
// finds the matching service and forwards the full SmcRequest to it.
// ---------------------------------------------------------------------------
class IScmService {
public:
    virtual ~IScmService() = default;

    // Owner group this service claims (e.g. 0x02 = SIP)
    virtual uint8_t owner()   const = 0;

    // Service ID this handler covers (e.g. 0x06 = BOOT service group)
    virtual uint8_t service() const = 0;

    // Handles the call. Returns SmcResult with handled=true if the command
    // was known, or handled=false for unknown commands within this service.
    virtual SmcResult handle(const SmcRequest& req) = 0;
};

} // namespace firmware
