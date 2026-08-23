#pragma once
#include <string>
#include <string_view>

namespace emulator::logger::models {

enum class Subsystem {
    LAUNCHER,
    ROM,
    BOOT,
    DTB,
    QEMU,
    GDB,
    STORAGE,
    SMC,
    ANDROID,
    BINDER,
    DISPLAY,
    DIAGNOSTICS,
    UNKNOWN
};

inline std::string_view ToString(Subsystem subsys) {
    switch (subsys) {
        case Subsystem::LAUNCHER:    return "LAUNCHER";
        case Subsystem::ROM:         return "ROM";
        case Subsystem::BOOT:        return "BOOT";
        case Subsystem::DTB:         return "DTB";
        case Subsystem::QEMU:        return "QEMU";
        case Subsystem::GDB:         return "GDB";
        case Subsystem::STORAGE:     return "STORAGE";
        case Subsystem::SMC:         return "SMC";
        case Subsystem::ANDROID:     return "ANDROID";
        case Subsystem::BINDER:      return "BINDER";
        case Subsystem::DISPLAY:     return "DISPLAY";
        case Subsystem::DIAGNOSTICS: return "DIAGNOSTICS";
        default:                     return "UNKNOWN";
    }
}

inline std::string_view ToLogFileName(Subsystem subsys) {
    switch (subsys) {
        case Subsystem::LAUNCHER:    return "launcher.log";
        case Subsystem::ROM:         return "rom.log";
        case Subsystem::BOOT:        return "boot.log";
        case Subsystem::DTB:         return "dtb.log";
        case Subsystem::QEMU:        return "qemu.log";
        case Subsystem::GDB:         return "gdb.log";
        case Subsystem::STORAGE:     return "storage.log";
        case Subsystem::SMC:         return "smc.log";
        case Subsystem::ANDROID:     return "android.log";
        case Subsystem::BINDER:      return "binder.log";
        case Subsystem::DISPLAY:     return "display.log";
        case Subsystem::DIAGNOSTICS: return "diagnostics.log";
        default:                     return "system.log";
    }
}

} // namespace emulator::logger::models
