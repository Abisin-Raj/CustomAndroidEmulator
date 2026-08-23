#include "input_event_record.h"
#include <sstream>

namespace emulator::input {

std::string InputEventRecord::ResolveTypeName(uint16_t type) {
    switch (static_cast<EventType>(type)) {
        case EventType::EV_SYN: return "EV_SYN";
        case EventType::EV_KEY: return "EV_KEY";
        case EventType::EV_REL: return "EV_REL";
        case EventType::EV_ABS: return "EV_ABS";
        case EventType::EV_MSC: return "EV_MSC";
        case EventType::EV_SW:  return "EV_SW";
        default:                return "EV_UNKNOWN";
    }
}

std::string InputEventRecord::ResolveCodeName(uint16_t type, uint16_t code) {
    if (type == static_cast<uint16_t>(EventType::EV_SYN)) {
        switch (static_cast<SynCode>(code)) {
            case SynCode::SYN_REPORT:    return "SYN_REPORT";
            case SynCode::SYN_CONFIG:    return "SYN_CONFIG";
            case SynCode::SYN_MT_REPORT: return "SYN_MT_REPORT";
            case SynCode::SYN_DROPPED:   return "SYN_DROPPED";
            default:                     return "SYN_UNKNOWN";
        }
    } else if (type == static_cast<uint16_t>(EventType::EV_ABS)) {
        switch (static_cast<AbsCode>(code)) {
            case AbsCode::ABS_X:              return "ABS_X";
            case AbsCode::ABS_Y:              return "ABS_Y";
            case AbsCode::ABS_PRESSURE:       return "ABS_PRESSURE";
            case AbsCode::ABS_MT_SLOT:        return "ABS_MT_SLOT";
            case AbsCode::ABS_MT_TOUCH_MAJOR: return "ABS_MT_TOUCH_MAJOR";
            case AbsCode::ABS_MT_POSITION_X:  return "ABS_MT_POSITION_X";
            case AbsCode::ABS_MT_POSITION_Y:  return "ABS_MT_POSITION_Y";
            case AbsCode::ABS_MT_TRACKING_ID: return "ABS_MT_TRACKING_ID";
            case AbsCode::ABS_MT_PRESSURE:    return "ABS_MT_PRESSURE";
            default:                          return "ABS_UNKNOWN";
        }
    } else if (type == static_cast<uint16_t>(EventType::EV_KEY)) {
        switch (static_cast<KeyCode>(code)) {
            case KeyCode::BTN_TOUCH:       return "BTN_TOUCH";
            case KeyCode::BTN_TOOL_FINGER: return "BTN_TOOL_FINGER";
            case KeyCode::KEY_POWER:       return "KEY_POWER";
            case KeyCode::KEY_VOLUMEDOWN:  return "KEY_VOLUMEDOWN";
            case KeyCode::KEY_VOLUMEUP:    return "KEY_VOLUMEUP";
            case KeyCode::KEY_HOMEPAGE:    return "KEY_HOMEPAGE";
            case KeyCode::KEY_BACK:        return "KEY_BACK";
            default:                       return "KEY_UNKNOWN";
        }
    }
    std::ostringstream ss;
    ss << "0x" << std::hex << code;
    return ss.str();
}

nlohmann::json InputEventRecord::ToJson() const {
    return nlohmann::json{
        {"timestamp_us", timestamp_us},
        {"type", type},
        {"type_name", type_name.empty() ? ResolveTypeName(type) : type_name},
        {"code", code},
        {"code_name", code_name.empty() ? ResolveCodeName(type, code) : code_name},
        {"value", value},
        {"target_window", target_window},
        {"dispatch_latency_ms", dispatch_latency_ms},
        {"status", status}
    };
}

} // namespace emulator::input
