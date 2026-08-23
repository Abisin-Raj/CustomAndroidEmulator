#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace emulator::storage {

enum class ScsiOpcode : uint8_t {
    TEST_UNIT_READY   = 0x00,
    REQUEST_SENSE     = 0x03,
    READ_6            = 0x08,
    WRITE_6           = 0x0A,
    INQUIRY           = 0x12,
    MODE_SENSE_6      = 0x1A,
    START_STOP_UNIT   = 0x1B,
    READ_10           = 0x28,
    WRITE_10          = 0x2A,
    SYNCHRONIZE_CACHE = 0x35,
    UNKNOWN           = 0xFF
};

inline std::string_view ScsiOpcodeToString(uint8_t op) {
    switch (static_cast<ScsiOpcode>(op)) {
        case ScsiOpcode::TEST_UNIT_READY:   return "TEST_UNIT_READY";
        case ScsiOpcode::REQUEST_SENSE:     return "REQUEST_SENSE";
        case ScsiOpcode::READ_6:            return "READ_6";
        case ScsiOpcode::WRITE_6:           return "WRITE_6";
        case ScsiOpcode::INQUIRY:           return "INQUIRY";
        case ScsiOpcode::MODE_SENSE_6:      return "MODE_SENSE_6";
        case ScsiOpcode::START_STOP_UNIT:   return "START_STOP_UNIT";
        case ScsiOpcode::READ_10:           return "READ10";
        case ScsiOpcode::WRITE_10:          return "WRITE10";
        case ScsiOpcode::SYNCHRONIZE_CACHE: return "SYNCHRONIZE_CACHE";
        default:                            return "SCSI_CUSTOM";
    }
}

struct ScsiCommandRecord {
    uint8_t opcode{0};
    std::string opcode_name;
    uint8_t lun{0};
    uint8_t slot{0};
    uint64_t lba{0};
    uint32_t blocks{0};
    uint64_t dma_address{0};
    double latency_ms{0.0};
    std::string result{"SUCCESS"};
    std::string timestamp;

    nlohmann::json ToJson() const;
    std::string FormatLog() const;
};

} // namespace emulator::storage
