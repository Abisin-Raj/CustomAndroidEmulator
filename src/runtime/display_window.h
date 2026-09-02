#pragma once
#include "framebuffer_surface.h"
#include "mouse_input_translator.h"
#include "keyboard_translator.h"
#include <string>
#include <memory>
#include <functional>
#include <vector>

namespace emulator::runtime {

using EventCallback = std::function<void(const std::vector<emulator::input::InputEventRecord>&)>;

struct ActiveTouchVisual {
    int32_t x{-1};
    int32_t y{-1};
    bool active{false};
    uint32_t ripple_radius{0};
};

class DisplayWindow {
public:
    DisplayWindow(const std::string& title = "Redmi Note 7 Pro (1080x2400 @ 60Hz)",
                  uint32_t width = 450, uint32_t height = 1000);
    ~DisplayWindow();

    bool Create();
    void Destroy();
    bool ProcessEvents(); // Returns false when window is closed
    void RenderFrame(const FramebufferSurface& surface);

    void SetInputCallback(EventCallback cb) { input_callback_ = cb; }
    void DispatchEvents(const std::vector<emulator::input::InputEventRecord>& events);

    bool IsOpen() const { return is_open_; }
    bool IsScreenOn() const { return is_screen_on_; }
    void ToggleScreen() { is_screen_on_ = !is_screen_on_; }

    uint32_t GetWindowWidth() const { return window_width_; }
    uint32_t GetWindowHeight() const { return window_height_; }

    MouseInputTranslator& GetMouseTranslator() { return mouse_translator_; }
    KeyboardTranslator& GetKeyboardTranslator() { return keyboard_translator_; }

    ActiveTouchVisual GetActiveTouch() const { return active_touch_; }
    void SetActiveTouch(int32_t x, int32_t y, bool active) {
        active_touch_.x = x;
        active_touch_.y = y;
        active_touch_.active = active;
        if (active) active_touch_.ripple_radius = 28;
    }

private:
    std::string title_;
    uint32_t window_width_{450};
    uint32_t window_height_{1000};
    bool is_open_{false};
    bool is_screen_on_{true};

    MouseInputTranslator mouse_translator_;
    KeyboardTranslator keyboard_translator_;
    EventCallback input_callback_;
    ActiveTouchVisual active_touch_;

    void* window_handle_{nullptr};
    void* device_context_{nullptr};
};

} // namespace emulator::runtime
