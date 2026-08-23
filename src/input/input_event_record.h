#pragma once
#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

namespace emulator::input {

// Standard Linux input event codes
enum class EventType : uint16_t {
    EV_SYN = 0x00,
    EV_KEY = 0x01,
    EV_REL = 0x02,
    EV_ABS = 0x03,
    EV_MSC = 0x04,
    EV_SW  = 0x05,
    EV_LED = 0x11,
    EV_SND = 0x12,
    EV_REP = 0x14,
    EV_FF  = 0x15
};

enum class AbsCode : uint16_t {
    ABS_X = 0x00,
    ABS_Y = 0x01,
    ABS_PRESSURE = 0x18,
    ABS_MT_SLOT = 0x2f,
    ABS_MT_TOUCH_MAJOR = 0x30,
    ABS_MT_TOUCH_MINOR = 0x31,
    ABS_MT_WIDTH_MAJOR = 0x32,
    ABS_MT_WIDTH_MINOR = 0x33,
    ABS_MT_ORIENTATION = 0x34,
    ABS_MT_POSITION_X = 0x35,
    ABS_MT_POSITION_Y = 0x36,
    ABS_MT_TOOL_TYPE = 0x37,
    ABS_MT_BLOB_ID = 0x38,
    ABS_MT_TRACKING_ID = 0x39,
    ABS_MT_PRESSURE = 0x3a
};

enum class KeyCode : uint16_t {
    BTN_TOUCH = 0x14a,
    BTN_TOOL_FINGER = 0x145,
    KEY_POWER = 116,
    KEY_VOLUMEDOWN = 114,
    KEY_VOLUMEUP = 115,
    KEY_HOMEPAGE = 172,
    KEY_BACK = 158
};

enum class SynCode : uint16_t {
    SYN_REPORT = 0x00,
    SYN_CONFIG = 0x01,
    SYN_MT_REPORT = 0x02,
    SYN_DROPPED = 0x03
};

struct InputEventRecord {
    uint64_t timestamp_us{0};
    uint16_t type{0};
    uint16_t code{0};
    int32_t value{0};

    std::string type_name;
    std::string code_name;
    std::string target_window;
    double dispatch_latency_ms{0.0};
    std::string status{"DISPATCHED"};

    nlohmann::json ToJson() const;
    static std::string ResolveTypeName(uint16_t type);
    static std::string ResolveCodeName(uint16_t type, uint16_t code);
};

} // namespace emulator::input
