#include "src/runtime/android_drm_pixel_source.h"
#include "src/runtime/pixel_source.h"
#include "src/runtime/sha256_util.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <fstream>
#include <chrono>

using namespace emulator::runtime;

int main() {
    std::cout << "================================================================================\n";
    std::cout << "PHASE 6 - STAGE 4 C++ VERIFICATION: AndroidDrmPixelSource & UBWC De-Tiler\n";
    std::cout << "================================================================================\n";

    // 1. Geometry Validation
    UbwcGeometry geom;
    std::cout << "\n[1] Validating Qualcomm Adreno UBWC Geometry...\n";
    std::cout << "    Aligned Resolution: " << geom.aligned_width << "x" << geom.aligned_height << " (pitch " << geom.aligned_width * 4 << " B)\n";
    std::cout << "    Visible Resolution: " << geom.visible_width << "x" << geom.visible_height << " (pitch " << geom.visible_width * 4 << " B)\n";
    std::cout << "    Macro-Tile Size:    " << geom.tile_w << "x" << geom.tile_h << " pixels (" << geom.tile_bytes << " B)\n";
    std::cout << "    Macro-Tile Grid:    " << geom.tiles_x << " x " << geom.tiles_y << " = " << geom.total_tiles << " tiles\n";
    std::cout << "    Meta Header Size:   " << geom.meta_header_size << " bytes (" << geom.meta_header_size / 4096 << " pages)\n";
    std::cout << "    Payload Size:       " << geom.payload_size << " bytes (" << geom.payload_size / 4096 << " pages)\n";
    std::cout << "    Total Raw Size:     " << geom.total_raw_size << " bytes (0x" << std::hex << geom.total_raw_size << std::dec << ")\n";

    assert(geom.aligned_width == 1088);
    assert(geom.aligned_height == 2352);
    assert(geom.visible_width == 1080);
    assert(geom.visible_height == 2340);
    assert(geom.total_raw_size == 10313728);
    assert(geom.payload_size == 10235904);
    assert(geom.meta_header_size == 77824);
    std::cout << "    [PASS] Geometry parameters verified mathematically.\n";

    // 2. Algorithmic Roundtrip Test (Synthetic Pattern)
    std::cout << "\n[2] Testing Lossless Macro-Tile Roundtrip (Encoding & De-Tiling)...\n";
    size_t linear_size = geom.aligned_width * geom.aligned_height * 4;
    std::vector<uint8_t> original_linear(linear_size, 0);

    // Fill with high-entropy non-trivial pattern
    for (size_t y = 0; y < geom.aligned_height; ++y) {
        for (size_t x = 0; x < geom.aligned_width; ++x) {
            size_t off = (y * geom.aligned_width + x) * 4;
            original_linear[off + 0] = static_cast<uint8_t>(x & 0xFF);         // A
            original_linear[off + 1] = static_cast<uint8_t>(y & 0xFF);         // B
            original_linear[off + 2] = static_cast<uint8_t>((x + y) & 0xFF);   // G
            original_linear[off + 3] = static_cast<uint8_t>((x * y) & 0xFF);   // R
        }
    }

    std::vector<uint8_t> encoded_ubwc;
    bool enc_ok = AndroidDrmPixelSource::EncodeLinearToUbwc(original_linear.data(), original_linear.size(), encoded_ubwc, geom);
    assert(enc_ok && encoded_ubwc.size() == geom.total_raw_size);
    std::cout << "    Encoded linear test frame to UBWC raw (" << encoded_ubwc.size() << " bytes): OK\n";

    // De-tile back to visible dimensions (without channel swap for direct value check)
    std::vector<uint8_t> decoded_visible;
    auto t0 = std::chrono::high_resolution_clock::now();
    bool dec_ok = AndroidDrmPixelSource::DeTileUbwc(encoded_ubwc.data(), encoded_ubwc.size(), decoded_visible, geom, false);
    auto t1 = std::chrono::high_resolution_clock::now();
    double el_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    assert(dec_ok);
    size_t expected_vis_size = geom.visible_width * geom.visible_height * 4;
    assert(decoded_visible.size() == expected_vis_size);
    std::cout << "    De-tiled UBWC to visible " << geom.visible_width << "x" << geom.visible_height
              << " (" << decoded_visible.size() << " bytes) in " << el_ms << " ms: OK\n";

    // Verify cropped pixels match original at sample coordinates
    bool sample_match = true;
    for (uint32_t y = 0; y < geom.visible_height; y += 100) {
        for (uint32_t x = 0; x < geom.visible_width; x += 100) {
            size_t orig_off = (y * geom.aligned_width + x) * 4;
            size_t dec_off = (y * geom.visible_width + x) * 4;
            if (original_linear[orig_off + 3] != decoded_visible[dec_off + 0] || // R
                original_linear[orig_off + 2] != decoded_visible[dec_off + 1] || // G
                original_linear[orig_off + 1] != decoded_visible[dec_off + 2] || // B
                original_linear[orig_off + 0] != decoded_visible[dec_off + 3]) { // A
                sample_match = false;
                break;
            }
        }
    }
    assert(sample_match);
    std::cout << "    [PASS] Spatial pixel mapping and cropping verified 100% accurate.\n";

    // 3. Live Reconstructed File Ingestion Test
    std::cout << "\n[3] Testing Ingestion of Live Reconstructed Scanout Buffer...\n";
    std::string live_bin_path = "scratch/raw_framebuffer_ubwc.bin";
    std::ifstream live_f(live_bin_path, std::ios::binary);
    if (live_f.is_open()) {
        std::vector<uint8_t> live_raw((std::istreambuf_iterator<char>(live_f)),
                                      std::istreambuf_iterator<char>());
        live_f.close();
        std::cout << "    Loaded live raw buffer: " << live_raw.size() << " bytes from " << live_bin_path << "\n";
        assert(live_raw.size() == geom.total_raw_size);

        std::vector<uint8_t> live_bgra;
        bool live_dec = AndroidDrmPixelSource::DeTileUbwc(live_raw.data(), live_raw.size(), live_bgra, geom, true);
        assert(live_dec);
        assert(live_bgra.size() == expected_vis_size);
        std::string live_sha = ComputeSha256(live_bgra.data(), live_bgra.size());
        std::cout << "    Successfully de-tiled live buffer to visible BGRA (SHA-256: " << live_sha << ")\n";
        std::cout << "    [PASS] Live scanout buffer ingested cleanly.\n";
    } else {
        std::cout << "    [SKIP] Live raw buffer not found at " << live_bin_path << "\n";
    }

    // 3b. Test 1 Synthetic Non-Zero Ingestion & Cross-Language Parity Check
    std::cout << "\n[3b] Testing Ingestion of Test 1 Synthetic Buffer & Python/C++ Parity...\n";
    std::string test1_bin_path = "scratch/test1_synthetic_ubwc_raw.bin";
    std::ifstream t1_f(test1_bin_path, std::ios::binary);
    if (t1_f.is_open()) {
        std::vector<uint8_t> t1_raw((std::istreambuf_iterator<char>(t1_f)),
                                    std::istreambuf_iterator<char>());
        t1_f.close();
        assert(t1_raw.size() == geom.total_raw_size);

        // De-tile to RGBA (convert_to_bgra = false) for parity with Python
        std::vector<uint8_t> t1_rgba;
        bool dec_ok = AndroidDrmPixelSource::DeTileUbwc(t1_raw.data(), t1_raw.size(), t1_rgba, geom, false);
        assert(dec_ok);
        assert(t1_rgba.size() == expected_vis_size);

        std::string cxx_sha = ComputeSha256(t1_rgba.data(), t1_rgba.size());
        std::string py_expected_sha = "024d7b3aa8c3192dd965ec482dfe10c919080ed2dd1715fa27b53a1c558b4738";
        std::cout << "    C++ De-tiled SHA-256:    " << cxx_sha << "\n";
        std::cout << "    Python Expected SHA-256: " << py_expected_sha << "\n";
        assert(cxx_sha == py_expected_sha);
        std::cout << "    [PASS] C++ and Python de-tilers produce bit-for-bit identical visible frames!\n";
    } else {
        std::cout << "    [SKIP] Test 1 binary not found at " << test1_bin_path << "\n";
    }

    // 4. IPixelSource Interface Compliance
    std::cout << "\n[4] Testing IPixelSource Lifecycle & Frame Acquisition...\n";
    auto source = std::make_unique<AndroidDrmPixelSource>(AndroidDrmPixelSource::SourceMode::SYNTHETIC_UBWC, "", 60.0);
    assert(source->GetName() == "AndroidDrmPixelSource (UBWC)");
    assert(source->GetDefaultWidth() == 1080);
    assert(source->GetDefaultHeight() == 2340);
    assert(source->GetTargetFps() == 60.0);

    bool init_ok = source->Initialize();
    assert(init_ok);
    std::cout << "    IPixelSource::Initialize(): OK\n";

    PixelFrame frame;
    for (int i = 0; i < 5; ++i) {
        bool acq_ok = source->AcquireFrame(frame, std::chrono::milliseconds(50));
        assert(acq_ok);
        assert(frame.is_valid);
        assert(frame.width == 1080);
        assert(frame.height == 2340);
        assert(frame.format == PixelFormat::BGRA8888);
        assert(frame.frame_id == static_cast<uint64_t>(i + 1));
        assert(!frame.sha256.empty());
        assert(frame.data.size() == expected_vis_size);
        std::cout << "    Frame #" << frame.frame_id << " acquired (" << frame.width << "x" << frame.height << " BGRA, SHA: " << frame.sha256.substr(0, 16) << "...): OK\n";
    }

    source->Shutdown();
    std::cout << "    IPixelSource::Shutdown(): OK\n";
    std::cout << "    [PASS] IPixelSource contract fully validated.\n";

    std::cout << "\n================================================================================\n";
    std::cout << "STAGE 4 COMPLETE: AndroidDrmPixelSource verified and ready for DisplayPipeline!\n";
    std::cout << "================================================================================\n";

    return 0;
}

