#pragma once

#include "pixel_source.h"
#include "framebuffer_surface.h"
#include "display_window.h"
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <string>

namespace emulator::runtime {

class DisplayPipeline {
public:
    DisplayPipeline(std::unique_ptr<IPixelSource> source,
                    const std::string& window_title = "Redmi Note 7 Pro (1080x1920 @ 60Hz - Android 13)",
                    uint32_t viewport_w = 450, uint32_t viewport_h = 800);
    ~DisplayPipeline();

    // Lifecycle
    bool Initialize();
    bool Start();
    void RunEventLoop();
    void Stop();
    bool IsRunning() const { return is_running_.load(); }

    // Component accessors
    IPixelSource* GetSource() { return source_.get(); }
    const IPixelSource* GetSource() const { return source_.get(); }
    FramebufferSurface& GetSurface() { return surface_; }
    const FramebufferSurface& GetSurface() const { return surface_; }
    DisplayWindow& GetWindow() { return window_; }
    const DisplayWindow& GetWindow() const { return window_; }

    // Callbacks
    void SetInputCallback(std::function<void(const std::vector<emulator::input::InputEventRecord>&)> cb) {
        window_.SetInputCallback(std::move(cb));
    }
    void SetStatsCallback(std::function<void(const FrameStats&)> cb) {
        stats_callback_ = std::move(cb);
    }

private:
    void AcquisitionWorker();

    std::unique_ptr<IPixelSource> source_;
    std::string window_title_;
    uint32_t viewport_w_{450};
    uint32_t viewport_h_{800};

    FramebufferSurface surface_;
    DisplayWindow window_;

    std::atomic<bool> is_running_{false};
    std::atomic<bool> worker_running_{false};
    std::thread acquisition_thread_;

    std::function<void(const FrameStats&)> stats_callback_;
};

} // namespace emulator::runtime

