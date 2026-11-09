#include "synthetic_pattern_pixel_source.h"
#include <cmath>
#include <thread>
#include <sstream>
#include <iomanip>

namespace emulator::runtime {

SyntheticPatternPixelSource::SyntheticPatternPixelSource(uint32_t width, uint32_t height,
                                                         double target_fps,
                                                         SyntheticPatternType pattern_type)
    : width_(width), height_(height), target_fps_(target_fps), pattern_type_(pattern_type) {}

SyntheticPatternPixelSource::~SyntheticPatternPixelSource() {
    Shutdown();
}

bool SyntheticPatternPixelSource::Initialize() {
    start_time_ = std::chrono::steady_clock::now();
    last_frame_time_ = start_time_;
    frame_counter_ = 0;
    initialized_.store(true);
    return true;
}

void SyntheticPatternPixelSource::Shutdown() {
    initialized_.store(false);
}

bool SyntheticPatternPixelSource::AcquireFrame(PixelFrame& out_frame, std::chrono::milliseconds timeout) {
    if (!initialized_.load()) {
        return false;
    }

    // Pace according to target_fps_
    auto now = std::chrono::steady_clock::now();
    double frame_interval_ms = 1000.0 / target_fps_;
    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - last_frame_time_).count();

    if (elapsed < frame_interval_ms) {
        double wait_ms = frame_interval_ms - elapsed;
        if (wait_ms > timeout.count() && timeout.count() > 0) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(wait_ms));
        now = std::chrono::steady_clock::now();
    }
    last_frame_time_ = now;

    uint64_t fid = ++frame_counter_;
    size_t buffer_size = width_ * height_ * 4;

    out_frame.width = width_;
    out_frame.height = height_;
    out_frame.format = PixelFormat::BGRA8888;
    out_frame.frame_id = fid;
    out_frame.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_time_).count();

    if (out_frame.data.size() != buffer_size) {
        out_frame.data.resize(buffer_size);
    }

    switch (pattern_type_) {
        case SyntheticPatternType::COLOR_BARS:
            RenderColorBars(out_frame.data, fid);
            break;
        case SyntheticPatternType::MOVING_GRADIENT:
            RenderMovingGradient(out_frame.data, fid);
            break;
        case SyntheticPatternType::GRID_SCAN:
            RenderGridScan(out_frame.data, fid);
            break;
    }

    out_frame.is_valid = true;
    return true;
}

void SyntheticPatternPixelSource::RenderColorBars(std::vector<uint8_t>& buffer, uint64_t frame_id) {
    // 7 standard SMPTE color bars (White, Yellow, Cyan, Green, Magenta, Red, Blue)
    static const uint8_t bars[7][3] = {
        {255, 255, 255}, // White (B, G, R)
        {0,   255, 255}, // Yellow
        {255, 255, 0  }, // Cyan
        {0,   255, 0  }, // Green
        {255, 0,   255}, // Magenta
        {0,   0,   255}, // Red
        {255, 0,   0  }  // Blue
    };

    uint32_t bar_width = width_ / 7;
    uint32_t bounce_y = static_cast<uint32_t>((std::sin(frame_id * 0.05) * 0.5 + 0.5) * (height_ - 100));

    for (uint32_t y = 0; y < height_; ++y) {
        uint8_t* row = buffer.data() + (y * width_ * 4);
        bool in_bounce_bar = (y >= bounce_y && y < bounce_y + 40);

        for (uint32_t x = 0; x < width_; ++x) {
            uint32_t bar_idx = std::min(x / bar_width, 6u);
            size_t idx = x * 4;

            if (in_bounce_bar) {
                // Moving horizontal indicator bar in dark gray/white
                row[idx + 0] = 220;
                row[idx + 1] = 220;
                row[idx + 2] = 220;
                row[idx + 3] = 255;
            } else {
                row[idx + 0] = bars[bar_idx][0]; // B
                row[idx + 1] = bars[bar_idx][1]; // G
                row[idx + 2] = bars[bar_idx][2]; // R
                row[idx + 3] = 255;
            }
        }
    }
}

void SyntheticPatternPixelSource::RenderMovingGradient(std::vector<uint8_t>& buffer, uint64_t frame_id) {
    // Smooth moving plasma/gradient with circular pulse
    double phase = frame_id * 0.04;
    double center_x = width_ * 0.5 + std::cos(phase) * (width_ * 0.25);
    double center_y = height_ * 0.5 + std::sin(phase * 0.7) * (height_ * 0.25);

    for (uint32_t y = 0; y < height_; y += 2) {
        for (uint32_t x = 0; x < width_; x += 2) {
            double dx = x - center_x;
            double dy = y - center_y;
            double dist = std::sqrt(dx * dx + dy * dy);

            uint8_t r = static_cast<uint8_t>((std::sin(dist * 0.02 - phase) * 0.5 + 0.5) * 230 + 20);
            uint8_t g = static_cast<uint8_t>((std::cos(x * 0.008 + phase * 0.5) * 0.5 + 0.5) * 200 + 30);
            uint8_t b = static_cast<uint8_t>((std::sin(y * 0.005 + phase * 0.8) * 0.5 + 0.5) * 240 + 15);

            // Fill 2x2 block for performance
            for (uint32_t by = 0; by < 2 && (y + by) < height_; ++by) {
                uint8_t* row = buffer.data() + ((y + by) * width_ * 4);
                for (uint32_t bx = 0; bx < 2 && (x + bx) < width_; ++bx) {
                    size_t idx = (x + bx) * 4;
                    row[idx + 0] = b;
                    row[idx + 1] = g;
                    row[idx + 2] = r;
                    row[idx + 3] = 255;
                }
            }
        }
    }
}

void SyntheticPatternPixelSource::RenderGridScan(std::vector<uint8_t>& buffer, uint64_t frame_id) {
    uint32_t scan_y = (frame_id * 8) % height_;
    uint32_t grid_spacing = 60;

    for (uint32_t y = 0; y < height_; ++y) {
        uint8_t* row = buffer.data() + (y * width_ * 4);
        bool is_scan_line = (y >= scan_y && y < scan_y + 6);
        bool is_grid_y = (y % grid_spacing == 0);

        for (uint32_t x = 0; x < width_; ++x) {
            bool is_grid_x = (x % grid_spacing == 0);
            size_t idx = x * 4;

            if (is_scan_line) {
                // Bright cyan scan line
                row[idx + 0] = 255;
                row[idx + 1] = 255;
                row[idx + 2] = 0;
                row[idx + 3] = 255;
            } else if (is_grid_x || is_grid_y) {
                // Dark blue grid
                row[idx + 0] = 120;
                row[idx + 1] = 40;
                row[idx + 2] = 20;
                row[idx + 3] = 255;
            } else {
                // Deep background
                row[idx + 0] = 20;
                row[idx + 1] = 10;
                row[idx + 2] = 10;
                row[idx + 3] = 255;
            }
        }
    }
}

} // namespace emulator::runtime

