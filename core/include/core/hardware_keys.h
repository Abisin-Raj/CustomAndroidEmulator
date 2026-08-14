#pragma once
#include <cstdint>

/**
 * @brief High-level hardware keys for mobile device physical buttons.
 * Disconnects the UI layer from guest OS input subsystem keycodes.
 */
enum class HardwareKey {
    VolumeUp,
    VolumeDown,
    Power
};
