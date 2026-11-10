#pragma once

#include "pixel_source.h"
#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <memory>
#include <functional>

namespace emulator::runtime {

struct UbwcGeometry {
    uint32_t aligned_width{1088};
    uint32_t aligned_height{2352};
    uint32_t visible_width{1080};
    uint32_t visible_height{2340};
    uint32_t bytes_per_pixel{4};

    uint32_t tile_w{16};
    uint32_t tile_h{4};
    uint32_t tile_bytes{256}; // 16 * 4 * 4

    uint32_t tiles_x{68};     // 1088 / 16
    uint32_t tiles_y{588};    // 2352 / 4
    uint32_t total_tiles{39984};

    size_t meta_header_size{77824}; // 19 * 4096
    size_t payload_size{10235904};  // 39984 * 256
    size_t total_raw_size{10313728}; // 0x9D6000
};

class AndroidDrmPixelSource : public IPixelSource {
public:
    enum class SourceMode {
        STORED_RAW_UBWC,    // Loads raw 10,313,728-byte buffer from file
        SYNTHETIC_UBWC,     // Generates dynamic 60fps UBWC macro-tiled frames
        LIVE_QMP_MEMORY     // Connects to live QEMU guest memory
    };

    explicit AndroidDrmPixelSource(SourceMode mode = SourceMode::STORED_RAW_UBWC,
                                   const std::string& raw_buffer_path = "scratch/raw_framebuffer_ubwc.bin",
                                   double target_fps = 60.0);
    ~AndroidDrmPixelSource() override;

    bool Initialize() override;
    void Shutdown() override;
    bool AcquireFrame(PixelFrame& out_frame, std::chrono::milliseconds timeout) override;

    std::string GetName() const override { return "AndroidDrmPixelSource (UBWC)"; }
    uint32_t GetDefaultWidth() const override { return geometry_.visible_width; }
    uint32_t GetDefaultHeight() const override { return geometry_.visible_height; }
    double GetTargetFps() const override { return target_fps_; }

    // UBWC De-Tiler Engine (Public static for unit testing & reuse)
    static bool DeTileUbwc(const uint8_t* raw_ubwc, size_t raw_len,
                           std::vector<uint8_t>& out_linear_bgra,
                           const UbwcGeometry& geom = UbwcGeometry(),
                           bool convert_to_bgra = true);

    // UBWC Encoder Engine (Generates macro-tiled frames for testing & validation)
    static bool EncodeLinearToUbwc(const uint8_t* linear_rgba, size_t linear_len,
                                   std::vector<uint8_t>& out_raw_ubwc,
                                   const UbwcGeometry& geom = UbwcGeometry());

    // Source configuration
    void SetRawBufferPath(const std::string& path);
    void SetMode(SourceMode mode) { mode_ = mode; }
    SourceMode GetMode() const { return mode_; }

private:
    void GenerateSyntheticUbwcFrame(std::vector<uint8_t>& out_raw, uint64_t frame_id);

    SourceMode mode_{SourceMode::STORED_RAW_UBWC};
    std::string raw_buffer_path_;
    double target_fps_{60.0};
    UbwcGeometry geometry_;

    std::atomic<bool> initialized_{false};
    uint64_t frame_counter_{0};
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_frame_time_;

    std::vector<uint8_t> cached_raw_buffer_;
    std::vector<uint8_t> decoded_frame_buffer_;
    std::string last_sha256_;
};

} // namespace emulator::runtime

