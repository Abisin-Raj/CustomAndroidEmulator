#include "mouse_input_translator.h"
#include <algorithm>

namespace emulator::runtime {

MouseInputTranslator::MouseInputTranslator(uint32_t native_width, uint32_t native_height)
    : native_width_(native_width), native_height_(native_height),
      win_width_(native_width), win_height_(native_height),
      touchscreen_(native_width, native_height) {}

void MouseInputTranslator::SetWindowViewport(uint32_t window_width, uint32_t window_height) {
    win_width_ = (window_width > 0) ? window_width : native_width_;
    win_height_ = (window_height > 0) ? window_height : native_height_;
}

TouchPoint MouseInputTranslator::MapWindowToTouch(int32_t win_x, int32_t win_y) const {
    if (win_width_ == 0 || win_height_ == 0) {
        return {0, 0, false};
    }

    // Preserve aspect ratio inside window viewport
    double native_aspect = static_cast<double>(native_width_) / native_height_;
    double window_aspect = static_cast<double>(win_width_) / win_height_;

    double render_w = win_width_;
    double render_h = win_height_;
    double offset_x = 0;
    double offset_y = 0;

    if (window_aspect > native_aspect) {
        // Pillarboxed (bars on left/right)
        render_w = win_height_ * native_aspect;
        offset_x = (win_width_ - render_w) / 2.0;
    } else {
        // Letterboxed (bars on top/bottom)
        render_h = win_width_ / native_aspect;
        offset_y = (win_height_ - render_h) / 2.0;
    }

    double local_x = win_x - offset_x;
    double local_y = win_y - offset_y;

    if (local_x < 0 || local_x > render_w || local_y < 0 || local_y > render_h) {
        return {0, 0, false}; // Clicked inside letterbox border
    }

    int32_t touch_x = static_cast<int32_t>((local_x / render_w) * native_width_);
    int32_t touch_y = static_cast<int32_t>((local_y / render_h) * native_height_);

    touch_x = std::clamp(touch_x, 0, static_cast<int32_t>(native_width_ - 1));
    touch_y = std::clamp(touch_y, 0, static_cast<int32_t>(native_height_ - 1));

    return {touch_x, touch_y, true};
}

std::vector<emulator::input::InputEventRecord> MouseInputTranslator::OnMouseDown(int32_t win_x, int32_t win_y, int32_t slot) {
    auto pt = MapWindowToTouch(win_x, win_y);
    if (!pt.is_valid) return {};

    is_dragging_ = true;
    last_touch_ = pt;
    return touchscreen_.CreateTouchDown(pt.x, pt.y, slot);
}

std::vector<emulator::input::InputEventRecord> MouseInputTranslator::OnMouseMove(int32_t win_x, int32_t win_y, int32_t slot) {
    if (!is_dragging_) return {};

    auto pt = MapWindowToTouch(win_x, win_y);
    if (!pt.is_valid) return {};

    last_touch_ = pt;
    return touchscreen_.CreateTouchMove(pt.x, pt.y, slot);
}

std::vector<emulator::input::InputEventRecord> MouseInputTranslator::OnMouseUp(int32_t slot) {
    if (!is_dragging_) return {};

    is_dragging_ = false;
    return touchscreen_.CreateTouchUp(slot);
}

std::vector<emulator::input::InputEventRecord> MouseInputTranslator::OnPinchZoom(int32_t center_x, int32_t center_y, float scale_factor) {
    auto pt = MapWindowToTouch(center_x, center_y);
    if (!pt.is_valid) return {};

    int32_t span = static_cast<int32_t>(200.0f * scale_factor);
    int32_t p1_x = std::clamp(pt.x - span, 0, static_cast<int32_t>(native_width_ - 1));
    int32_t p1_y = std::clamp(pt.y - span, 0, static_cast<int32_t>(native_height_ - 1));
    int32_t p2_x = std::clamp(pt.x + span, 0, static_cast<int32_t>(native_width_ - 1));
    int32_t p2_y = std::clamp(pt.y + span, 0, static_cast<int32_t>(native_height_ - 1));

    // Multi-finger gesture with Slot 0 and Slot 1
    auto f1 = touchscreen_.CreateTouchDown(p1_x, p1_y, 0, 10);
    auto f2 = touchscreen_.CreateTouchDown(p2_x, p2_y, 1, 11);
    f1.insert(f1.end(), f2.begin(), f2.end());
    return f1;
}

} // namespace emulator::runtime
