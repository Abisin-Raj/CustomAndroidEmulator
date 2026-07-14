#include "image_converter/brotli_decompressor.h"
#include <brotli/decode.h>
#include <fstream>
#include <vector>

static constexpr size_t kBufSize = 65536; // 64 KiB streaming chunks

bool BrotliDecompressor::decompress(const std::filesystem::path& input,
                                    const std::filesystem::path& output,
                                    std::function<void(float)>   progressCb,
                                    std::string*                 errorOut)
{
    std::ifstream in(input, std::ios::binary | std::ios::ate);
    if (!in) {
        if (errorOut) *errorOut = "Cannot open input: " + input.string();
        return false;
    }
    const uint64_t totalBytes = static_cast<uint64_t>(in.tellg());
    in.seekg(0);

    std::ofstream out(output, std::ios::binary);
    if (!out) {
        if (errorOut) *errorOut = "Cannot open output: " + output.string();
        return false;
    }

    BrotliDecoderState* state = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
    if (!state) {
        if (errorOut) *errorOut = "Failed to create Brotli decoder state.";
        return false;
    }

    std::vector<uint8_t> inBuf(kBufSize);
    std::vector<uint8_t> outBuf(kBufSize * 4);

    uint64_t bytesRead = 0;
    BrotliDecoderResult result = BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT;

    while (result != BROTLI_DECODER_RESULT_SUCCESS) {
        if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
            in.read(reinterpret_cast<char*>(inBuf.data()), static_cast<std::streamsize>(kBufSize));
            std::streamsize got = in.gcount();
            if (got <= 0 && result != BROTLI_DECODER_RESULT_SUCCESS) {
                if (errorOut) *errorOut = "Unexpected end of brotli input.";
                BrotliDecoderDestroyInstance(state);
                return false;
            }
            bytesRead += static_cast<uint64_t>(got);
            if (progressCb && totalBytes > 0) {
                progressCb(static_cast<float>(bytesRead) / static_cast<float>(totalBytes));
            }

            const uint8_t* nextIn  = inBuf.data();
            size_t         availIn = static_cast<size_t>(got);

            while (availIn > 0 || result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
                uint8_t* nextOut  = outBuf.data();
                size_t   availOut = outBuf.size();

                result = BrotliDecoderDecompressStream(state, &availIn, &nextIn, &availOut, &nextOut, nullptr);

                size_t written = outBuf.size() - availOut;
                if (written > 0) {
                    out.write(reinterpret_cast<const char*>(outBuf.data()), static_cast<std::streamsize>(written));
                }

                if (result == BROTLI_DECODER_RESULT_ERROR) {
                    if (errorOut) {
                        *errorOut = "Brotli decode error: ";
                        *errorOut += BrotliDecoderErrorString(BrotliDecoderGetErrorCode(state));
                    }
                    BrotliDecoderDestroyInstance(state);
                    return false;
                }
                if (result == BROTLI_DECODER_RESULT_SUCCESS) break;
                if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) break;
            }
        } else if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
            // Already handled in the inner loop above
        } else {
            break;
        }
    }

    BrotliDecoderDestroyInstance(state);
    if (progressCb) progressCb(1.0f);
    return true;
}
