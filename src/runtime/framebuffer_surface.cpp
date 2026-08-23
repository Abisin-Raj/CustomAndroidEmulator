#include "framebuffer_surface.h"
#include <thread>
#include <cstring>
#include <algorithm>

namespace emulator::runtime {

FramebufferSurface::FramebufferSurface(uint32_t width, uint32_t height)
    : width_(width), height_(height),
      front_buffer_(width * height * 4, 0x1e), // Dark theme default
      back_buffer_(width * height * 4, 0x1e),
      last_vsync_time_(std::chrono::steady_clock::now()) {}

void FramebufferSurface::SetOrientation(Orientation orient) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (orientation_ == orient) return;

    bool was_portrait = (orientation_ == Orientation::PORTRAIT || orientation_ == Orientation::PORTRAIT_REVERSE);
    bool now_portrait = (orient == Orientation::PORTRAIT || orient == Orientation::PORTRAIT_REVERSE);

    if (was_portrait != now_portrait) {
        std::swap(width_, height_);
    }
    orientation_ = orient;
    front_buffer_.assign(width_ * height_ * 4, 0x1e);
    back_buffer_.assign(width_ * height_ * 4, 0x1e);
}

void FramebufferSurface::Rotate90() {
    switch (orientation_) {
        case Orientation::PORTRAIT:          SetOrientation(Orientation::LANDSCAPE); break;
        case Orientation::LANDSCAPE:         SetOrientation(Orientation::PORTRAIT_REVERSE); break;
        case Orientation::PORTRAIT_REVERSE:  SetOrientation(Orientation::LANDSCAPE_REVERSE); break;
        case Orientation::LANDSCAPE_REVERSE: SetOrientation(Orientation::PORTRAIT); break;
    }
}

void FramebufferSurface::UpdateFrontBuffer(const uint8_t* rgba_data, size_t size) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    size_t copy_size = std::min(size, front_buffer_.size());
    std::memcpy(front_buffer_.data(), rgba_data, copy_size);
}

std::vector<uint8_t> FramebufferSurface::GetBackBuffer() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return back_buffer_;
}

void FramebufferSurface::SwapBuffers() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    std::swap(front_buffer_, back_buffer_);
}

void FramebufferSurface::WaitForVsync() {
    if (frame_count_ == 0) {
        last_vsync_time_ = std::chrono::steady_clock::now();
        current_fps_ = 60.0;
    }

    auto target_time = last_vsync_time_ + std::chrono::microseconds(16666); // Exactly 60.0 Hz
    auto now = std::chrono::steady_clock::now();

    if (now < target_time) {
        auto remaining_us = std::chrono::duration_cast<std::chrono::microseconds>(target_time - now).count();
        if (remaining_us > 2000) {
            std::this_thread::sleep_for(std::chrono::microseconds(remaining_us - 1500));
        }
        // Precision spin-wait for final sub-millisecond accuracy
        while (std::chrono::steady_clock::now() < target_time) {
            std::this_thread::yield();
        }
    }

    auto post_vsync = std::chrono::steady_clock::now();
    double frame_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(post_vsync - last_vsync_time_).count() / 1000.0;
    if (frame_time_ms > 0.0) {
        double instant_fps = 1000.0 / frame_time_ms;
        current_fps_ = (frame_count_ == 0) ? 60.0 : (0.8 * current_fps_ + 0.2 * instant_fps);
    }
    last_vsync_time_ = post_vsync;
    frame_count_++;
}



double FramebufferSurface::GetCurrentFps() const {
    return current_fps_;
}

} // namespace emulator::runtime
