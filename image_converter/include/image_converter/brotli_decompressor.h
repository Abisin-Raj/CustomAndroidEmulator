#pragma once
#include <filesystem>
#include <functional>
#include <string>

/// Decompresses a Brotli-compressed file (.new.dat.br) to a raw file (.new.dat).
class BrotliDecompressor {
public:
    /// Decompress @p input to @p output.
    /// @param progressCb  Called with [0.0, 1.0] progress fraction as bytes are consumed.
    ///                    Pass nullptr for no progress reporting.
    /// @return true on success, false on error (error message written to @p errorOut).
    static bool decompress(const std::filesystem::path& input,
                           const std::filesystem::path& output,
                           std::function<void(float)>   progressCb = nullptr,
                           std::string*                 errorOut   = nullptr);
};
