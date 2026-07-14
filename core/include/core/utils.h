#pragma once
#include <concepts>
#include <bit>
#include <cstdint>

namespace core {

/**
 * @brief Portable byteswap for integral types
 */
template <typename T>
requires std::integral<T>
constexpr T byteswap(T val) {
    if constexpr (sizeof(T) == 1) {
        return val;
    } else if constexpr (sizeof(T) == 2) {
        return static_cast<T>((static_cast<uint16_t>(val) >> 8) | (static_cast<uint16_t>(val) << 8));
    } else if constexpr (sizeof(T) == 4) {
        uint32_t v = static_cast<uint32_t>(val);
        return static_cast<T>(((v & 0xff000000) >> 24) |
                              ((v & 0x00ff0000) >> 8)  |
                              ((v & 0x0000ff00) << 8)  |
                              ((v & 0x000000ff) << 24));
    } else if constexpr (sizeof(T) == 8) {
        uint64_t v = static_cast<uint64_t>(val);
        return static_cast<T>(((v & 0xff00000000000000ULL) >> 56) |
                              ((v & 0x00ff000000000000ULL) >> 40) |
                              ((v & 0x0000ff0000000000ULL) >> 24) |
                              ((v & 0x000000ff00000000ULL) >> 8)  |
                              ((v & 0x00000000ff000000ULL) << 8)  |
                              ((v & 0x0000000000ff0000ULL) << 24) |
                              ((v & 0x000000000000ff00ULL) << 40) |
                              ((v & 0x00000000000000ffULL) << 56));
    } else {
        return val;
    }
}

/**
 * @brief Converts Big Endian to Host order
 */
template <typename T>
requires std::integral<T>
constexpr T beToHost(T val) {
    if constexpr (std::endian::native == std::endian::little) {
        return byteswap(val);
    } else {
        return val;
    }
}

/**
 * @brief Converts Little Endian to Host order
 */
template <typename T>
requires std::integral<T>
constexpr T leToHost(T val) {
    if constexpr (std::endian::native == std::endian::big) {
        return byteswap(val);
    } else {
        return val;
    }
}

} // namespace core
