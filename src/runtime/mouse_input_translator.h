#pragma once
#include "src/input/virtual_touchscreen.h"
#include "src/input/input_event_record.h"
#include <vector>
#include <cstdint>

namespace emulator::runtime {

struct TouchPoint {
    int32_t x{0};
    int32_t y{0};
    bool is_valid{false};
};

class MouseInputTranslator {
public:
    MouseInputTranslator(uint32_t native_width = 1080, uint32_t native_height = 2400);

    void SetWindowViewport(uint32_t window_width, uint32_t window_height);
    TouchPoint MapWindowToTouch(int32_t win_x, int32_t win_y) const;

    std::vector<emulator::input::InputEventRecord> OnMouseDown(int32_t win_x, int32_t win_y, int32_t slot = 0);
    std::vector<emulator::input::InputEventRecord> OnMouseMove(int32_t win_x, int32_t win_y, int32_t slot = 0);
    std::vector<emulator::input::InputEventRecord> OnMouseUp(int32_t slot = 0);
    std::vector<emulator::input::InputEventRecord> OnPinchZoom(int32_t center_x, int32_t center_y, float scale_factor);

    bool IsDragging() const { return is_dragging_; }

private:
    uint32_t native_width_{1080};
    uint32_t native_height_{2400};
    uint32_t win_width_{1080};
    uint32_t win_height_{2400};

    bool is_dragging_{false};
    TouchPoint last_touch_;
    emulator::input::VirtualTouchscreen touchscreen_;
};

} // namespace emulator::runtime
