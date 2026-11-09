#include "display_pipeline.h"
#include <iostream>

namespace emulator::runtime {

DisplayPipeline::DisplayPipeline(std::unique_ptr<IPixelSource> source,
                                 const std::string& window_title,
                                 uint32_t viewport_w, uint32_t viewport_h)
    : source_(std::move(source)),
      window_title_(window_title),
      viewport_w_(viewport_w),
      viewport_h_(viewport_h),
      surface_(source_ ? source_->GetDefaultWidth() : 1080,
               source_ ? source_->GetDefaultHeight() : 1920),
      window_(window_title_, viewport_w_, viewport_h_) {}

DisplayPipeline::~DisplayPipeline() {
    Stop();
}

bool DisplayPipeline::Initialize() {
    if (!source_) {
        std::cerr << "[-] DisplayPipeline: No pixel source attached.\n";
        return false;
    }

    if (!source_->Initialize()) {
        std::cerr << "[-] DisplayPipeline: Failed to initialize pixel source: " << source_->GetName() << "\n";
        return false;
    }

    if (!window_.Create()) {
        std::cerr << "[-] DisplayPipeline: Failed to create host DisplayWindow.\n";
        source_->Shutdown();
        return false;
    }

    // Initialize surface with a clean blank frame
    size_t initial_size = surface_.GetWidth() * surface_.GetHeight() * 4;
    std::vector<uint8_t> blank(initial_size, 0);
    surface_.UpdateFrontBuffer(blank.data(), blank.size());
    surface_.SwapBuffers();
    window_.RenderFrame(surface_);

    return true;
}

bool DisplayPipeline::Start() {
    if (is_running_.load()) {
        return true;
    }

    if (!source_ || !window_.IsOpen()) {
        if (!Initialize()) {
            return false;
        }
    }

    is_running_.store(true);
    worker_running_.store(true);
    acquisition_thread_ = std::thread(&DisplayPipeline::AcquisitionWorker, this);

    return true;
}

void DisplayPipeline::AcquisitionWorker() {
    PixelFrame frame;
    while (worker_running_.load()) {
        if (source_ && source_->AcquireFrame(frame, std::chrono::milliseconds(50))) {
            if (frame.is_valid && !frame.data.empty()) {
                surface_.UpdateFrontBuffer(frame.data.data(), frame.data.size(),
                                           frame.frame_id, frame.sha256);
                surface_.SwapBuffers();
            }
        }
    }
}

void DisplayPipeline::RunEventLoop() {
    if (!is_running_.load()) {
        if (!Start()) {
            return;
        }
    }

    FrameStats stats;
    while (is_running_.load() && window_.ProcessEvents()) {
        surface_.WaitForVsync();
        window_.RenderFrame(surface_);

        if (surface_.HasNewSecondStats(stats) && stats_callback_) {
            stats_callback_(stats);
        }
    }

    Stop();
}

void DisplayPipeline::Stop() {
    if (!is_running_.exchange(false)) {
        return;
    }

    worker_running_.store(false);
    if (acquisition_thread_.joinable()) {
        acquisition_thread_.join();
    }

    if (source_) {
        source_->Shutdown();
    }

    window_.Destroy();
}

} // namespace emulator::runtime

