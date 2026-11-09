#pragma once

#include "pixel_source.h"
#include <atomic>
#include <chrono>

namespace emulator::runtime {

enum class SyntheticPatternType {
    COLOR_BARS,
    MOVING_GRADIENT,
    GRID_SCAN
};

class SyntheticPatternPixelSource : public IPixelSource {
public:
    SyntheticPatternPixelSource(uint32_t width = 1080, uint32_t height = 1920,
                               double target_fps = 60.0,
                               SyntheticPatternType pattern_type = SyntheticPatternType::MOVING_GRADIENT);
    ~SyntheticPatternPixelSource() override;

    bool Initialize() override;
    void Shutdown() override;
    bool AcquireFrame(PixelFrame& out_frame, std::chrono::milliseconds timeout) override;

    std::string GetName() const override { return "SyntheticPatternPixelSource"; }
    uint32_t GetDefaultWidth() const override { return width_; }
    uint32_t GetDefaultHeight() const override { return height_; }
    double GetTargetFps() const override { return target_fps_; }

    void SetPatternType(SyntheticPatternType type) { pattern_type_ = type; }

private:
    void RenderColorBars(std::vector<uint8_t>& buffer, uint64_t frame_id);
    void RenderMovingGradient(std::vector<uint8_t>& buffer, uint64_t frame_id);
    void RenderGridScan(std::vector<uint8_t>& buffer, uint64_t frame_id);

    uint32_t width_{1080};
    uint32_t height_{1920};
    double target_fps_{60.0};
    SyntheticPatternType pattern_type_{SyntheticPatternType::MOVING_GRADIENT};

    std::atomic<bool> initialized_{false};
    uint64_t frame_counter_{0};
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_frame_time_;
};

} // namespace emulator::runtime

