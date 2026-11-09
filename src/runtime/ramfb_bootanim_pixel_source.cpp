#include "ramfb_bootanim_pixel_source.h"
#include "sha256_util.h"
#include "src/gdb/gdb_session.h"
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <thread>
#include <iostream>

namespace emulator::runtime {

RamfbBootAnimPixelSource::RamfbBootAnimPixelSource(uint32_t width, uint32_t height,
                                                   double target_fps,
                                                   const std::string& bootanim_dir)
    : width_(width), height_(height), target_fps_(target_fps), bootanim_dir_(bootanim_dir) {}

RamfbBootAnimPixelSource::~RamfbBootAnimPixelSource() {
    Shutdown();
}

void RamfbBootAnimPixelSource::SetBootAnimDir(const std::string& dir_path) {
    bootanim_dir_ = dir_path;
}

void RamfbBootAnimPixelSource::SetGdbSession(emulator::gdb::GdbSession* session, uint64_t phys_addr) {
    gdb_session_ = session;
    phys_addr_ = phys_addr;
}

bool RamfbBootAnimPixelSource::Initialize() {
    start_time_ = std::chrono::steady_clock::now();
    last_frame_time_ = start_time_;
    frame_counter_ = 0;

    // 1. Try loading offline raw sequence frames
    bool loaded = LoadOfflineSequence(bootanim_dir_);
    if (!loaded || cached_frames_.empty()) {
        GenerateFallbackFrames(20);
    }

    initialized_.store(true);
    return true;
}

void RamfbBootAnimPixelSource::Shutdown() {
    initialized_.store(false);
}

bool RamfbBootAnimPixelSource::DecodeUbwcRawFrame(const uint8_t* raw_data, size_t raw_len,
                                                  std::vector<uint8_t>& out_bgra,
                                                  uint32_t target_w, uint32_t target_h) {
    constexpr size_t kMetaPlaneSize = 77824;
    constexpr uint32_t kBufW = 1088;
    constexpr uint32_t kBufH = 2352;
    constexpr uint32_t kTileW = 16;
    constexpr uint32_t kTileH = 4;
    constexpr uint32_t kTilesX = kBufW / kTileW; // 68

    if (raw_len < kMetaPlaneSize + (kBufW * kBufH * 4)) {
        return false;
    }

    const uint8_t* payload = raw_data + kMetaPlaneSize;
    size_t payload_len = raw_len - kMetaPlaneSize;

    out_bgra.assign(target_w * target_h * 4, 0);

    uint32_t max_y = std::min(target_h, 2340u);
    uint32_t max_x = std::min(target_w, 1080u);

    for (uint32_t y = 0; y < max_y; ++y) {
        uint32_t ty = y / kTileH;
        uint32_t py = y % kTileH;

        for (uint32_t x = 0; x < max_x; ++x) {
            uint32_t tx = x / kTileW;
            uint32_t px = x % kTileW;

            uint32_t tile_idx = ty * kTilesX + tx;
            size_t pix_off = tile_idx * 256 + (py * kTileW + px) * 4;

            if (pix_off + 3 < payload_len) {
                size_t ci = (y * target_w + x) * 4;
                out_bgra[ci + 0] = payload[pix_off + 2]; // B
                out_bgra[ci + 1] = payload[pix_off + 1]; // G
                out_bgra[ci + 2] = payload[pix_off + 0]; // R
                out_bgra[ci + 3] = payload[pix_off + 3]; // A
            }
        }
    }
    return true;
}

bool RamfbBootAnimPixelSource::LoadOfflineSequence(const std::string& dir_path) {
    std::error_code ec;
    if (!std::filesystem::exists(dir_path, ec) || !std::filesystem::is_directory(dir_path, ec)) {
        return false;
    }

    cached_frames_.clear();
    cached_frame_hashes_.clear();

    std::vector<std::string> raw_files;
    for (const auto& entry : std::filesystem::directory_iterator(dir_path, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".raw") {
            raw_files.push_back(entry.path().string());
        }
    }

    if (raw_files.empty()) {
        return false;
    }

    std::sort(raw_files.begin(), raw_files.end());

    std::vector<uint8_t> raw_buf;
    std::vector<uint8_t> bgra_buf;

    for (const auto& filepath : raw_files) {
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) continue;

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        raw_buf.resize(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(raw_buf.data()), size)) {
            continue;
        }

        if (DecodeUbwcRawFrame(raw_buf.data(), raw_buf.size(), bgra_buf, width_, height_)) {
            std::string sha = ComputeSha256(bgra_buf.data(), bgra_buf.size());
            cached_frames_.push_back(bgra_buf);
            cached_frame_hashes_.push_back(sha);
        }
    }

    return !cached_frames_.empty();
}

void RamfbBootAnimPixelSource::GenerateFallbackFrames(size_t count) {
    cached_frames_.clear();
    cached_frame_hashes_.clear();

    size_t buffer_size = width_ * height_ * 4;

    // Material You palette colors: Red, Green, Blue, Yellow (BGRA order)
    const uint8_t colors[4][4] = {
        { 53,  67, 234, 255}, // Red:    #ea4335
        { 83, 168,  52, 255}, // Green:  #34a853
        {244, 133,  66, 255}, // Blue:   #4285f4
        {  4, 188, 251, 255}  // Yellow: #fbbc04
    };

    int cx = static_cast<int>(width_ / 2);
    int cy = static_cast<int>(height_ / 2);
    int base_radius = 18;

    for (size_t f = 0; f < count; ++f) {
        std::vector<uint8_t> frame(buffer_size, 0);
        double angle_offset = (2.0 * 3.141592653589793 / count) * f;

        for (int i = 0; i < 4; ++i) {
            double angle = angle_offset + (i * 3.141592653589793 / 2.0);
            double dist = 60.0 + 20.0 * std::sin(angle_offset * 2.0 + i);
            int dot_x = cx + static_cast<int>(dist * std::cos(angle));
            int dot_y = cy + static_cast<int>(dist * std::sin(angle));

            int r = base_radius;
            for (int dy = -r; dy <= r; ++dy) {
                int py = dot_y + dy;
                if (py < 0 || py >= static_cast<int>(height_)) continue;
                for (int dx = -r; dx <= r; ++dx) {
                    int px = dot_x + dx;
                    if (px < 0 || px >= static_cast<int>(width_)) continue;
                    if (dx * dx + dy * dy <= r * r) {
                        size_t idx = (py * width_ + px) * 4;
                        frame[idx + 0] = colors[i][0];
                        frame[idx + 1] = colors[i][1];
                        frame[idx + 2] = colors[i][2];
                        frame[idx + 3] = colors[i][3];
                    }
                }
            }
        }

        std::string sha = ComputeSha256(frame.data(), frame.size());
        cached_frames_.push_back(std::move(frame));
        cached_frame_hashes_.push_back(sha);
    }
}

bool RamfbBootAnimPixelSource::AcquireFrame(PixelFrame& out_frame, std::chrono::milliseconds timeout) {
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

    size_t expected_size = width_ * height_ * 4;

    // 1. Check if live GDB session is connected and has non-zero frame data
    if (gdb_session_ && gdb_session_->IsConnected()) {
        auto live_bytes = gdb_session_->ReadMemory(phys_addr_, expected_size);
        if (live_bytes.size() == expected_size) {
            size_t non_zero_bytes = 0;
            for (size_t i = 0; i < std::min(live_bytes.size(), size_t(8192)); ++i) {
                if (live_bytes[i] != 0) non_zero_bytes++;
            }

            if (non_zero_bytes > 0) {
                std::string sha = ComputeSha256(live_bytes.data(), live_bytes.size());
                uint64_t fid = ++frame_counter_;
                last_live_sha256_ = sha;
                last_live_frame_id_.store(fid);
                gdb_active_.store(true);

                out_frame.width = width_;
                out_frame.height = height_;
                out_frame.format = PixelFormat::BGRA8888;
                out_frame.frame_id = fid;
                out_frame.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_time_).count();
                out_frame.sha256 = sha;
                out_frame.data = std::move(live_bytes);
                out_frame.is_valid = true;
                return true;
            }
        }
    }

    gdb_active_.store(false);

    // 2. Fall back to sequence carousel playback
    if (cached_frames_.empty()) {
        GenerateFallbackFrames(20);
    }

    uint64_t fid = ++frame_counter_;
    size_t frame_idx = (fid - 1) % cached_frames_.size();

    out_frame.width = width_;
    out_frame.height = height_;
    out_frame.format = PixelFormat::BGRA8888;
    out_frame.frame_id = fid;
    out_frame.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_time_).count();
    out_frame.sha256 = cached_frame_hashes_[frame_idx];
    out_frame.data = cached_frames_[frame_idx];
    out_frame.is_valid = true;

    return true;
}

} // namespace emulator::runtime

