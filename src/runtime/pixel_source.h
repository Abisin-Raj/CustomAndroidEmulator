#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace emulator::runtime {

enum class PixelFormat {
    RGBA8888,
    BGRA8888,
    RGB565,
    UBWC_RGBA8888
};

struct PixelFrame {
    uint32_t width{1080};
    uint32_t height{1920};
    PixelFormat format{PixelFormat::BGRA8888};
    uint64_t frame_id{0};
    uint64_t timestamp_ns{0};
    std::string sha256;
    std::vector<uint8_t> data;
    bool is_valid{false};
};

class IPixelSource {
public:
    virtual ~IPixelSource() = default;

    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual bool AcquireFrame(PixelFrame& out_frame, std::chrono::milliseconds timeout) = 0;

    virtual std::string GetName() const = 0;
    virtual uint32_t GetDefaultWidth() const = 0;
    virtual uint32_t GetDefaultHeight() const = 0;
    virtual double GetTargetFps() const = 0;
};

} // namespace emulator::runtime

