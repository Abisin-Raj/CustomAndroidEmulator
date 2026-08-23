#pragma once
#include "input_event_record.h"
#include <vector>
#include <cstdint>

namespace emulator::input {

class VirtualTouchscreen {
public:
    explicit VirtualTouchscreen(uint32_t width = 1080, uint32_t height = 2400);

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }

    std::vector<InputEventRecord> CreateTouchDown(int32_t x, int32_t y, int32_t slot = 0, int32_t tracking_id = 1);
    std::vector<InputEventRecord> CreateTouchMove(int32_t x, int32_t y, int32_t slot = 0);
    std::vector<InputEventRecord> CreateTouchUp(int32_t slot = 0);
    std::vector<InputEventRecord> CreateTap(int32_t x, int32_t y);

    std::vector<InputEventRecord> CreateKeyPress(uint16_t key_code, bool down);

private:
    uint32_t width_{1080};
    uint32_t height_{2400};
    int32_t current_slot_{0};
    int32_t current_tracking_id_{1};
};

} // namespace emulator::input
