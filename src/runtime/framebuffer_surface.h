#pragma once
#include <cstdint>
#include <vector>
#include <mutex>
#include <string>
#include <chrono>
#include <nlohmann/json.hpp>

namespace emulator::runtime {

enum class Orientation {
    PORTRAIT,
    LANDSCAPE,
    PORTRAIT_REVERSE,
    LANDSCAPE_REVERSE
};

struct FrameStats {
    uint32_t frames_presented{0};
    uint32_t frames_dropped{0};
    double avg_present_time_ms{16.67};
    double worst_frame_time_ms{16.67};
    double jitter_ms{0.25};
    double measured_fps{60.0};

    std::string FormatLog() const;
    nlohmann::json ToJson() const;
};

class FramebufferSurface {
public:
    FramebufferSurface(uint32_t width = 1080, uint32_t height = 2400);

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    Orientation GetOrientation() const { return orientation_; }

    void SetOrientation(Orientation orient);
    void Rotate90();

    // Double buffering with provenance tracking
    void UpdateFrontBuffer(const uint8_t* rgba_data, size_t size, uint64_t frame_id = 0, const std::string& sha256 = "");
    std::vector<uint8_t> GetBackBuffer() const;
    uint64_t GetBackBufferFrameId() const;
    std::string GetBackBufferSha256() const;
    void SwapBuffers();

    // VSYNC pacing (60 Hz target = 16.666 ms)
    void WaitForVsync();
    double GetCurrentFps() const;
    FrameStats GetFrameStats() const;
    bool HasNewSecondStats(FrameStats& out_stats);

private:
    uint32_t width_{1080};
    uint32_t height_{2400};
    Orientation orientation_{Orientation::PORTRAIT};

    mutable std::mutex buffer_mutex_;
    std::vector<uint8_t> front_buffer_;
    std::vector<uint8_t> back_buffer_;
    uint64_t front_frame_id_{0};
    uint64_t back_frame_id_{0};
    std::string front_sha256_{""};
    std::string back_sha256_{""};

    std::chrono::steady_clock::time_point last_vsync_time_;
    std::chrono::steady_clock::time_point next_vsync_target_;
    std::chrono::steady_clock::time_point second_start_time_;
    uint64_t frame_count_{0};


    // Rolling second window statistics
    uint32_t window_presented_{0};
    uint32_t window_dropped_{0};
    std::vector<double> window_frame_times_;
    FrameStats latest_stats_;
    bool new_stats_ready_{false};
};

} // namespace emulator::runtime
