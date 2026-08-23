#include "framebuffer_surface.h"
#include <thread>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace emulator::runtime {

std::string FrameStats::FormatLog() const {
    std::ostringstream ss;
    ss << "Frames Presented: " << frames_presented
       << " | Frames Dropped: " << frames_dropped
       << " | Avg Present Time: " << std::fixed << std::setprecision(2) << avg_present_time_ms << " ms"
       << " | Worst Frame: " << worst_frame_time_ms << " ms"
       << " | Jitter: ±" << jitter_ms << " ms"
       << " | Pacing: " << std::setprecision(1) << measured_fps << " FPS";
    return ss.str();
}

nlohmann::json FrameStats::ToJson() const {
    return nlohmann::json{
        {"frames_presented", frames_presented},
        {"frames_dropped", frames_dropped},
        {"avg_present_time_ms", avg_present_time_ms},
        {"worst_frame_time_ms", worst_frame_time_ms},
        {"jitter_ms", jitter_ms},
        {"measured_fps", measured_fps}
    };
}

FramebufferSurface::FramebufferSurface(uint32_t width, uint32_t height)
    : width_(width), height_(height),
      front_buffer_(width * height * 4, 0x1e),
      back_buffer_(width * height * 4, 0x1e),
      last_vsync_time_(std::chrono::steady_clock::now()),
      second_start_time_(std::chrono::steady_clock::now()) {}

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
    auto now = std::chrono::steady_clock::now();

    if (frame_count_ == 0) {
        next_vsync_target_ = now + std::chrono::microseconds(16666);
        last_vsync_time_ = now;
        second_start_time_ = now;
        window_frame_times_.clear();
        window_presented_ = 0;
        window_dropped_ = 0;
    } else {
        next_vsync_target_ += std::chrono::microseconds(16666);
    }

    now = std::chrono::steady_clock::now();
    if (now < next_vsync_target_) {
        auto remaining_us = std::chrono::duration_cast<std::chrono::microseconds>(next_vsync_target_ - now).count();
        if (remaining_us > 2000) {
            std::this_thread::sleep_for(std::chrono::microseconds(remaining_us - 1200));
        }
        while (std::chrono::steady_clock::now() < next_vsync_target_) {
            std::this_thread::yield();
        }
    } else {
        if (now > next_vsync_target_ + std::chrono::microseconds(33333)) {
            next_vsync_target_ = now;
        }
    }

    auto post_vsync = std::chrono::steady_clock::now();
    double frame_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(post_vsync - last_vsync_time_).count() / 1000.0;
    last_vsync_time_ = post_vsync;
    frame_count_++;

    window_presented_++;
    window_frame_times_.push_back(frame_time_ms);
    if (frame_time_ms > 20.0) {
        window_dropped_++;
    }

    auto sec_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(post_vsync - second_start_time_).count();
    if (sec_elapsed_ms >= 1000 || window_presented_ >= 60) {
        double sum = std::accumulate(window_frame_times_.begin(), window_frame_times_.end(), 0.0);
        double avg = window_frame_times_.empty() ? 16.67 : (sum / window_frame_times_.size());
        double worst = 0.0;
        for (double t : window_frame_times_) {
            if (t > worst) worst = t;
        }

        double variance_sum = 0.0;
        for (double t : window_frame_times_) {
            variance_sum += (t - avg) * (t - avg);
        }
        double std_dev = window_frame_times_.empty() ? 0.25 : std::sqrt(variance_sum / window_frame_times_.size());

        latest_stats_.frames_presented = window_presented_;
        latest_stats_.frames_dropped = window_dropped_;
        latest_stats_.avg_present_time_ms = avg;
        latest_stats_.worst_frame_time_ms = worst;
        latest_stats_.jitter_ms = std_dev;
        latest_stats_.measured_fps = (sec_elapsed_ms > 0) ? (window_presented_ * 1000.0 / sec_elapsed_ms) : 60.0;

        new_stats_ready_ = true;
        second_start_time_ = post_vsync;
        window_presented_ = 0;
        window_dropped_ = 0;
        window_frame_times_.clear();
    }
}


double FramebufferSurface::GetCurrentFps() const {
    return latest_stats_.measured_fps > 0 ? latest_stats_.measured_fps : 60.0;
}

FrameStats FramebufferSurface::GetFrameStats() const {
    return latest_stats_;
}

bool FramebufferSurface::HasNewSecondStats(FrameStats& out_stats) {
    if (new_stats_ready_) {
        out_stats = latest_stats_;
        new_stats_ready_ = false;
        return true;
    }
    return false;
}

} // namespace emulator::runtime
