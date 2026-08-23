#include "virtual_touchscreen.h"
#include <chrono>

namespace emulator::input {

static uint64_t GetTimestampUs() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

VirtualTouchscreen::VirtualTouchscreen(uint32_t width, uint32_t height)
    : width_(width), height_(height) {}

std::vector<InputEventRecord> VirtualTouchscreen::CreateTouchDown(int32_t x, int32_t y, int32_t slot, int32_t tracking_id) {
    current_slot_ = slot;
    current_tracking_id_ = tracking_id;
    uint64_t ts = GetTimestampUs();

    std::vector<InputEventRecord> events;

    // 1. ABS_MT_SLOT
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_ABS), static_cast<uint16_t>(AbsCode::ABS_MT_SLOT), slot, "EV_ABS", "ABS_MT_SLOT", "", 0.0, "INJECTED"});
    // 2. ABS_MT_TRACKING_ID
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_ABS), static_cast<uint16_t>(AbsCode::ABS_MT_TRACKING_ID), tracking_id, "EV_ABS", "ABS_MT_TRACKING_ID", "", 0.0, "INJECTED"});
    // 3. ABS_MT_POSITION_X
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_ABS), static_cast<uint16_t>(AbsCode::ABS_MT_POSITION_X), x, "EV_ABS", "ABS_MT_POSITION_X", "", 0.0, "INJECTED"});
    // 4. ABS_MT_POSITION_Y
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_ABS), static_cast<uint16_t>(AbsCode::ABS_MT_POSITION_Y), y, "EV_ABS", "ABS_MT_POSITION_Y", "", 0.0, "INJECTED"});
    // 5. BTN_TOUCH = 1
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_KEY), static_cast<uint16_t>(KeyCode::BTN_TOUCH), 1, "EV_KEY", "BTN_TOUCH", "", 0.0, "INJECTED"});
    // 6. SYN_REPORT
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_SYN), static_cast<uint16_t>(SynCode::SYN_REPORT), 0, "EV_SYN", "SYN_REPORT", "", 0.0, "INJECTED"});

    return events;
}

std::vector<InputEventRecord> VirtualTouchscreen::CreateTouchMove(int32_t x, int32_t y, int32_t slot) {
    uint64_t ts = GetTimestampUs();
    std::vector<InputEventRecord> events;

    events.push_back({ts, static_cast<uint16_t>(EventType::EV_ABS), static_cast<uint16_t>(AbsCode::ABS_MT_SLOT), slot, "EV_ABS", "ABS_MT_SLOT", "", 0.0, "INJECTED"});
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_ABS), static_cast<uint16_t>(AbsCode::ABS_MT_POSITION_X), x, "EV_ABS", "ABS_MT_POSITION_X", "", 0.0, "INJECTED"});
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_ABS), static_cast<uint16_t>(AbsCode::ABS_MT_POSITION_Y), y, "EV_ABS", "ABS_MT_POSITION_Y", "", 0.0, "INJECTED"});
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_SYN), static_cast<uint16_t>(SynCode::SYN_REPORT), 0, "EV_SYN", "SYN_REPORT", "", 0.0, "INJECTED"});

    return events;
}

std::vector<InputEventRecord> VirtualTouchscreen::CreateTouchUp(int32_t slot) {
    uint64_t ts = GetTimestampUs();
    std::vector<InputEventRecord> events;

    // 1. ABS_MT_SLOT
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_ABS), static_cast<uint16_t>(AbsCode::ABS_MT_SLOT), slot, "EV_ABS", "ABS_MT_SLOT", "", 0.0, "INJECTED"});
    // 2. ABS_MT_TRACKING_ID = -1 (finger lifted)
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_ABS), static_cast<uint16_t>(AbsCode::ABS_MT_TRACKING_ID), -1, "EV_ABS", "ABS_MT_TRACKING_ID", "", 0.0, "INJECTED"});
    // 3. BTN_TOUCH = 0
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_KEY), static_cast<uint16_t>(KeyCode::BTN_TOUCH), 0, "EV_KEY", "BTN_TOUCH", "", 0.0, "INJECTED"});
    // 4. SYN_REPORT
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_SYN), static_cast<uint16_t>(SynCode::SYN_REPORT), 0, "EV_SYN", "SYN_REPORT", "", 0.0, "INJECTED"});

    return events;
}

std::vector<InputEventRecord> VirtualTouchscreen::CreateTap(int32_t x, int32_t y) {
    auto down = CreateTouchDown(x, y);
    auto up = CreateTouchUp();
    down.insert(down.end(), up.begin(), up.end());
    return down;
}

std::vector<InputEventRecord> VirtualTouchscreen::CreateKeyPress(uint16_t key_code, bool down) {
    uint64_t ts = GetTimestampUs();
    std::vector<InputEventRecord> events;

    events.push_back({ts, static_cast<uint16_t>(EventType::EV_KEY), key_code, down ? 1 : 0, "EV_KEY", InputEventRecord::ResolveCodeName(static_cast<uint16_t>(EventType::EV_KEY), key_code), "", 0.0, "INJECTED"});
    events.push_back({ts, static_cast<uint16_t>(EventType::EV_SYN), static_cast<uint16_t>(SynCode::SYN_REPORT), 0, "EV_SYN", "SYN_REPORT", "", 0.0, "INJECTED"});

    return events;
}

} // namespace emulator::input
