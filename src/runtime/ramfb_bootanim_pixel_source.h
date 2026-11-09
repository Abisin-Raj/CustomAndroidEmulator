#pragma once

#include "pixel_source.h"
#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <memory>

namespace emulator::gdb {
class GdbSession;
}

namespace emulator::runtime {

class RamfbBootAnimPixelSource : public IPixelSource {
public:
    RamfbBootAnimPixelSource(uint32_t width = 1080, uint32_t height = 1920,
                             double target_fps = 60.0,
                             const std::string& bootanim_dir = "C:/qemu_work/bootanim_frames");
    ~RamfbBootAnimPixelSource() override;

    bool Initialize() override;
    void Shutdown() override;
    bool AcquireFrame(PixelFrame& out_frame, std::chrono::milliseconds timeout) override;

    std::string GetName() const override { return "RamfbBootAnimPixelSource"; }
    uint32_t GetDefaultWidth() const override { return width_; }
    uint32_t GetDefaultHeight() const override { return height_; }
    double GetTargetFps() const override { return target_fps_; }

    // Configuration & Inspection
    void SetBootAnimDir(const std::string& dir_path);
    std::string GetBootAnimDir() const { return bootanim_dir_; }
    size_t GetLoadedFrameCount() const { return cached_frames_.size(); }

    void SetGdbSession(emulator::gdb::GdbSession* session, uint64_t phys_addr = 0x7F000000ULL);
    bool IsLiveGdbActive() const { return gdb_active_.load(); }
    uint64_t GetLastLiveFrameId() const { return last_live_frame_id_.load(); }

    // Sequence loading & decoding helpers
    bool LoadOfflineSequence(const std::string& dir_path);

private:
    void GenerateFallbackFrames(size_t count = 20);
    static bool DecodeUbwcRawFrame(const uint8_t* raw_data, size_t raw_len,
                                   std::vector<uint8_t>& out_bgra,
                                   uint32_t target_w, uint32_t target_h);

    uint32_t width_{1080};
    uint32_t height_{1920};
    double target_fps_{60.0};
    std::string bootanim_dir_;

    std::atomic<bool> initialized_{false};
    std::atomic<bool> gdb_active_{false};
    emulator::gdb::GdbSession* gdb_session_{nullptr};
    uint64_t phys_addr_{0x7F000000ULL};

    std::vector<std::vector<uint8_t>> cached_frames_;
    std::vector<std::string> cached_frame_hashes_;

    uint64_t frame_counter_{0};
    std::atomic<uint64_t> last_live_frame_id_{0};
    std::string last_live_sha256_;

    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_frame_time_;
};

} // namespace emulator::runtime

