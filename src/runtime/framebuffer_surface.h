#pragma once
#include <cstdint>
#include <vector>
#include <mutex>
#include <string>
#include <chrono>

namespace emulator::runtime {

enum class Orientation {
    PORTRAIT,
    LANDSCAPE,
    PORTRAIT_REVERSE,
    LANDSCAPE_REVERSE
};

class FramebufferSurface {
public:
    FramebufferSurface(uint32_t width = 1080, uint32_t height = 2400);

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    Orientation GetOrientation() const { return orientation_; }

    void SetOrientation(Orientation orient);
    void Rotate90();

    // Double buffering
    void UpdateFrontBuffer(const uint8_t* rgba_data, size_t size);
    std::vector<uint8_t> GetBackBuffer() const;
    void SwapBuffers();

    // VSYNC pacing (60 Hz target = 16.666 ms)
    void WaitForVsync();
    double GetCurrentFps() const;

private:
    uint32_t width_{1080};
    uint32_t height_{2400};
    Orientation orientation_{Orientation::PORTRAIT};

    mutable std::mutex buffer_mutex_;
    std::vector<uint8_t> front_buffer_;
    std::vector<uint8_t> back_buffer_;

    std::chrono::steady_clock::time_point last_vsync_time_;
    double current_fps_{60.0};
    uint64_t frame_count_{0};
};

} // namespace emulator::runtime
