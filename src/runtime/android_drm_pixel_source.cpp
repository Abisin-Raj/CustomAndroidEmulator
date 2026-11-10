#include "android_drm_pixel_source.h"
#include "sha256_util.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <thread>
#include <algorithm>
#include <cmath>

namespace emulator::runtime {

AndroidDrmPixelSource::AndroidDrmPixelSource(SourceMode mode,
                                             const std::string& raw_buffer_path,
                                             double target_fps)
    : mode_(mode),
      raw_buffer_path_(raw_buffer_path),
      target_fps_(target_fps) {}

AndroidDrmPixelSource::~AndroidDrmPixelSource() {
    Shutdown();
}

bool AndroidDrmPixelSource::Initialize() {
    if (initialized_.load()) {
        return true;
    }

    start_time_ = std::chrono::steady_clock::now();
    last_frame_time_ = start_time_;
    frame_counter_ = 0;

    size_t visible_size = geometry_.visible_width * geometry_.visible_height * geometry_.bytes_per_pixel;
    decoded_frame_buffer_.resize(visible_size, 0);

    if (mode_ == SourceMode::STORED_RAW_UBWC) {
        std::ifstream file(raw_buffer_path_, std::ios::binary);
        if (file.is_open()) {
            cached_raw_buffer_.assign((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());
            file.close();
            std::cout << "[+] AndroidDrmPixelSource: Loaded " << cached_raw_buffer_.size()
                      << " bytes from " << raw_buffer_path_ << "\n";

            if (cached_raw_buffer_.size() == geometry_.total_raw_size) {
                DeTileUbwc(cached_raw_buffer_.data(), cached_raw_buffer_.size(),
                           decoded_frame_buffer_, geometry_, true);
                last_sha256_ = ComputeSha256(decoded_frame_buffer_.data(), decoded_frame_buffer_.size());
                std::cout << "[+] AndroidDrmPixelSource: De-tiled scanout buffer to "
                          << geometry_.visible_width << "x" << geometry_.visible_height
                          << " (SHA-256: " << last_sha256_ << ")\n";
            } else {
                std::cerr << "[-] AndroidDrmPixelSource: File size mismatch: "
                          << cached_raw_buffer_.size() << " != expected " << geometry_.total_raw_size
                          << " (falling back to synthetic mode)\n";
                mode_ = SourceMode::SYNTHETIC_UBWC;
            }
        } else {
            std::cout << "[!] AndroidDrmPixelSource: Cannot open " << raw_buffer_path_
                      << " (switching to SYNTHETIC_UBWC generator mode)\n";
            mode_ = SourceMode::SYNTHETIC_UBWC;
        }
    }

    initialized_.store(true);
    return true;
}

void AndroidDrmPixelSource::Shutdown() {
    initialized_.store(false);
}

void AndroidDrmPixelSource::SetRawBufferPath(const std::string& path) {
    raw_buffer_path_ = path;
    if (initialized_.load() && mode_ == SourceMode::STORED_RAW_UBWC) {
        initialized_.store(false);
        Initialize();
    }
}

bool AndroidDrmPixelSource::AcquireFrame(PixelFrame& out_frame, std::chrono::milliseconds timeout) {
    if (!initialized_.load()) {
        return false;
    }

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

    if (mode_ == SourceMode::SYNTHETIC_UBWC) {
        std::vector<uint8_t> synthetic_raw;
        GenerateSyntheticUbwcFrame(synthetic_raw, fid);
        DeTileUbwc(synthetic_raw.data(), synthetic_raw.size(),
                   decoded_frame_buffer_, geometry_, true);
        last_sha256_ = ComputeSha256(decoded_frame_buffer_.data(), decoded_frame_buffer_.size());
    }

    out_frame.width = geometry_.visible_width;
    out_frame.height = geometry_.visible_height;
    out_frame.format = PixelFormat::BGRA8888;
    out_frame.frame_id = fid;
    out_frame.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_time_).count();
    out_frame.sha256 = last_sha256_;
    out_frame.data = decoded_frame_buffer_;
    out_frame.is_valid = true;

    return true;
}

bool AndroidDrmPixelSource::DeTileUbwc(const uint8_t* raw_ubwc, size_t raw_len,
                                      std::vector<uint8_t>& out_linear_bgra,
                                      const UbwcGeometry& geom,
                                      bool convert_to_bgra) {
    if (!raw_ubwc || raw_len != geom.total_raw_size) {
        return false;
    }

    size_t visible_bytes = geom.visible_width * geom.visible_height * geom.bytes_per_pixel;
    if (out_linear_bgra.size() != visible_bytes) {
        out_linear_bgra.resize(visible_bytes);
    }

    const uint8_t* payload = raw_ubwc + geom.meta_header_size;
    uint8_t* dst = out_linear_bgra.data();

    const uint32_t tile_w = geom.tile_w;
    const uint32_t tile_h = geom.tile_h;
    const uint32_t tiles_x = geom.tiles_x;
    const uint32_t tiles_y = geom.tiles_y;
    const uint32_t vis_w = geom.visible_width;
    const uint32_t vis_h = geom.visible_height;

    for (uint32_t ty = 0; ty < tiles_y; ++ty) {
        uint32_t y_base = ty * tile_h;
        if (y_base >= vis_h) {
            break; // Entire tile row is outside visible area (bottom cropping)
        }

        for (uint32_t tx = 0; tx < tiles_x; ++tx) {
            uint32_t x_base = tx * tile_w;
            uint32_t tile_idx = ty * tiles_x + tx;
            const uint8_t* tile_ptr = payload + tile_idx * geom.tile_bytes;

            for (uint32_t r = 0; r < tile_h; ++r) {
                uint32_t py = y_base + r;
                if (py >= vis_h) continue;

                for (uint32_t c = 0; c < tile_w; ++c) {
                    uint32_t px = x_base + c;
                    if (px >= vis_w) continue; // Outside visible area (right margin cropping)

                    const uint8_t* src_px = tile_ptr + (r * tile_w + c) * 4;
                    uint8_t* dst_px = dst + (py * vis_w + px) * 4;

                    if (convert_to_bgra) {
                        // DRM ABGR8888 in memory: b0=A, b1=B, b2=G, b3=R
                        // Windows DIB BGRA8888:   b0=B, b1=G, b2=R, b3=A
                        dst_px[0] = src_px[1]; // B
                        dst_px[1] = src_px[2]; // G
                        dst_px[2] = src_px[3]; // R
                        dst_px[3] = src_px[0]; // A
                    } else {
                        // Direct RGBA8888: b0=R, b1=G, b2=B, b3=A
                        dst_px[0] = src_px[3]; // R
                        dst_px[1] = src_px[2]; // G
                        dst_px[2] = src_px[1]; // B
                        dst_px[3] = src_px[0]; // A
                    }
                }
            }
        }
    }

    return true;
}

bool AndroidDrmPixelSource::EncodeLinearToUbwc(const uint8_t* linear_rgba, size_t linear_len,
                                               std::vector<uint8_t>& out_raw_ubwc,
                                               const UbwcGeometry& geom) {
    size_t expected_linear = geom.aligned_width * geom.aligned_height * geom.bytes_per_pixel;
    if (!linear_rgba || linear_len != expected_linear) {
        return false;
    }

    out_raw_ubwc.assign(geom.total_raw_size, 0); // 77,824 B zero metadata header
    uint8_t* payload = out_raw_ubwc.data() + geom.meta_header_size;

    const uint32_t tile_w = geom.tile_w;
    const uint32_t tile_h = geom.tile_h;
    const uint32_t tiles_x = geom.tiles_x;
    const uint32_t tiles_y = geom.tiles_y;
    const uint32_t aligned_w = geom.aligned_width;

    for (uint32_t ty = 0; ty < tiles_y; ++ty) {
        uint32_t y_base = ty * tile_h;
        for (uint32_t tx = 0; tx < tiles_x; ++tx) {
            uint32_t x_base = tx * tile_w;
            uint32_t tile_idx = ty * tiles_x + tx;
            uint8_t* tile_ptr = payload + tile_idx * geom.tile_bytes;

            for (uint32_t r = 0; r < tile_h; ++r) {
                uint32_t py = y_base + r;
                for (uint32_t c = 0; c < tile_w; ++c) {
                    uint32_t px = x_base + c;

                    const uint8_t* src_px = linear_rgba + (py * aligned_w + px) * 4;
                    uint8_t* dst_px = tile_ptr + (r * tile_w + c) * 4;

                    // Copy raw 4-byte pixel into macro-tile
                    dst_px[0] = src_px[0];
                    dst_px[1] = src_px[1];
                    dst_px[2] = src_px[2];
                    dst_px[3] = src_px[3];
                }
            }
        }
    }

    return true;
}

void AndroidDrmPixelSource::GenerateSyntheticUbwcFrame(std::vector<uint8_t>& out_raw, uint64_t frame_id) {
    size_t aligned_size = geometry_.aligned_width * geometry_.aligned_height * 4;
    std::vector<uint8_t> linear(aligned_size, 0);

    const uint32_t stride = geometry_.aligned_width * 4;
    const uint32_t W = geometry_.aligned_width;
    const uint32_t H = geometry_.aligned_height;

    // Background dark charcoal (#121212) in DRM ABGR format: A=0xff, B=0x12, G=0x12, R=0x12
    uint8_t bg_a = 0xff, bg_b = 0x12, bg_g = 0x12, bg_r = 0x12;
    for (size_t i = 0; i < aligned_size; i += 4) {
        linear[i + 0] = bg_a;
        linear[i + 1] = bg_b;
        linear[i + 2] = bg_g;
        linear[i + 3] = bg_r;
    }

    auto draw_rect = [&](uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
                         uint8_t a, uint8_t b, uint8_t g, uint8_t r) {
        for (uint32_t y = y0; y < y1 && y < H; ++y) {
            for (uint32_t x = x0; x < x1 && x < W; ++x) {
                size_t off = y * stride + x * 4;
                linear[off + 0] = a;
                linear[off + 1] = b;
                linear[off + 2] = g;
                linear[off + 3] = r;
            }
        }
    };

    // Android Green Status Bar (#3DDC84): R=61, G=220, B=132
    draw_rect(0, 0, W, 120, 0xff, 132, 220, 61);

    // Dynamic pulsating wave in the middle
    uint32_t scan_y = 300 + static_cast<uint32_t>((frame_id * 6) % 1500);
    draw_rect(60, scan_y, W - 60, scan_y + 40, 0xff, 244, 133, 66); // Google Blue

    // Floating cards
    draw_rect(80, 400, 500, 900, 0xff, 53, 67, 234);    // Google Red
    draw_rect(580, 400, 1000, 900, 0xff, 5, 188, 251);   // Google Yellow
    draw_rect(80, 1000, 500, 1500, 0xff, 212, 188, 0);   // Cyan
    draw_rect(580, 1000, 1000, 1500, 0xff, 132, 220, 61);// Green

    // Navigation bar (#202124)
    draw_rect(0, H - 140, W, H, 0xff, 0x24, 0x21, 0x20);

    // Encode to macro-tile UBWC format
    EncodeLinearToUbwc(linear.data(), linear.size(), out_raw, geometry_);
}

} // namespace emulator::runtime

