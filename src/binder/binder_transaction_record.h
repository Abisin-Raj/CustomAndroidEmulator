#pragma once
#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

namespace emulator::binder {

enum class BinderCommand : uint32_t {
    BC_TRANSACTION          = 0x40406300,
    BC_REPLY                = 0x40406301,
    BC_ACQUIRE_RESULT       = 0x40046302,
    BC_FREE_BUFFER          = 0x40086303,
    BC_INCREFS              = 0x40046304,
    BC_ACQUIRE              = 0x40046305,
    BC_RELEASE              = 0x40046306,
    BC_DECREFS              = 0x40046307,
    BC_ENTER_LOOPER         = 0x0000630c,
    BC_EXIT_LOOPER          = 0x0000630d,
    UNKNOWN                 = 0xFFFFFFFF
};

enum class BinderReturn : uint32_t {
    BR_ERROR                = 0x80047200,
    BR_OK                   = 0x00007201,
    BR_TRANSACTION          = 0x80407202,
    BR_REPLY                = 0x80407203,
    BR_ACQUIRE_RESULT       = 0x80047204,
    BR_DEAD_REPLY           = 0x00007205,
    BR_TRANSACTION_COMPLETE = 0x00007206,
    BR_INCREFS              = 0x80087207,
    BR_ACQUIRE              = 0x80087208,
    BR_RELEASE              = 0x80087209,
    BR_DECREFS              = 0x8008720a,
    BR_NOOP                 = 0x0000720c,
    BR_SPAWN_LOOPER         = 0x0000720d,
    BR_DEAD_BINDER          = 0x8008720f,
    BR_FAILED_REPLY         = 0x00007211,
    UNKNOWN                 = 0xFFFFFFFF
};

struct BinderTransactionRecord {
    uint32_t pid{0};
    uint32_t tid{0};
    std::string command{"BC_TRANSACTION"};
    std::string reply_status{"BR_TRANSACTION_COMPLETE"};
    uint32_t handle{0};
    uint32_t code{0};
    std::string interface_name;
    size_t payload_bytes{0};
    double latency_ms{0.0};
    std::string result{"SUCCESS"};
    std::string timestamp;

    nlohmann::json ToJson() const;
    std::string FormatCommandLog() const;
    std::string FormatReplyLog() const;

    static std::string ResolveInterface(uint32_t handle, uint32_t code, const std::string& token = "");
};

} // namespace emulator::binder
