#pragma once
#include <cstdint>
#include <string>

namespace firmware {

// ---------------------------------------------------------------------------
// SMC Identifier decoder
//
// AArch64 SMC Calling Convention (SMCCC):
//   Bit[31]      = 0 = Yielding Call, 1 = Fast Call
//   Bit[30]      = 0 = SMC32 (AArch32), 1 = SMC64 (AArch64)
//   Bits[29:24]  = Owning Entity (0x02 = SIP — Silicon-specific)
//   Bits[23:16]  = (reserved/unused in QCOM)
//   Bits[15:8]   = Service ID
//   Bits[7:0]    = Command ID
// ---------------------------------------------------------------------------
struct SmcId {
    uint32_t raw;

    bool isFastCall()  const { return (raw >> 31) & 0x1; }
    bool is64bit()     const { return (raw >> 30) & 0x1; }
    uint8_t owner()    const { return (raw >> 24) & 0x3F; }
    uint8_t service()  const { return (raw >> 8)  & 0xFF; }
    uint8_t command()  const { return (raw >> 0)  & 0xFF; }

    std::string toString() const;
};

// ---------------------------------------------------------------------------
// A full SMC call request (x0 encodes the SmcId, x1-x6 are arguments)
// ---------------------------------------------------------------------------
struct SmcRequest {
    SmcId   id;
    uint64_t args[6] = {0, 0, 0, 0, 0, 0}; // x1 - x6
};

// ---------------------------------------------------------------------------
// Result returned from a handler. x0 is the primary return register.
// SMCCC convention: 0 = Success, 0xFFFFFFFFFFFFFFFF = NOT_SUPPORTED
// ---------------------------------------------------------------------------
struct SmcResult {
    uint64_t x0 = 0xFFFFFFFFFFFFFFFFULL; // Default: NOT_SUPPORTED
    bool     handled = false;
};

// Well-known SMCCC return codes
constexpr uint64_t SMCCC_RET_SUCCESS       = 0ULL;
constexpr uint64_t SMCCC_RET_NOT_SUPPORTED = 0xFFFFFFFFFFFFFFFFULL;
constexpr uint64_t SMCCC_RET_NOT_REQUIRED  = 0xFFFFFFFFFFFFFFFEULL;
constexpr uint64_t SMCCC_RET_INVALID_PARAM = 0xFFFFFFFFFFFFFFFDULL;

} // namespace firmware
