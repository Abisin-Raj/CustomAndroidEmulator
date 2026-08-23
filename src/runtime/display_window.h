#pragma once
#include "framebuffer_surface.h"
#include "mouse_input_translator.h"
#include "keyboard_translator.h"
#include <string>
#include <memory>
#include <functional>

namespace emulator::runtime {

using EventCallback = std::function<void(const std::vector<emulator::input::InputEventRecord>&)>;

class DisplayWindow {
public:
    DisplayWindow(const std::string& title = "Redmi Note 7 Pro Emulator (1080x2400)",
                  uint32_t width = 450, uint32_t height = 1000);
    ~DisplayWindow();

    bool Create();
    void Destroy();
    bool ProcessEvents(); // Returns false if window closed (WM_QUIT / close)
    void RenderFrame(const FramebufferSurface& surface);

    void SetInputCallback(EventCallback cb) { input_callback_ = cb; }
    bool IsOpen() const { return is_open_; }

    uint32_t GetWindowWidth() const { return window_width_; }
    uint32_t GetWindowHeight() const { return window_height_; }

    MouseInputTranslator& GetMouseTranslator() { return mouse_translator_; }
    KeyboardTranslator& GetKeyboardTranslator() { return keyboard_translator_; }

private:
    std::string title_;
    uint32_t window_width_{450};
    uint32_t window_height_{1000};
    bool is_open_{false};

    MouseInputTranslator mouse_translator_;
    KeyboardTranslator keyboard_translator_;
    EventCallback input_callback_;

    void* window_handle_{nullptr};
    void* device_context_{nullptr};
};

} // namespace emulator::runtime
