#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <nlohmann/json.hpp>

namespace emulator::logger::models {

enum class ExitReason {
    SUCCESS,
    CRASH,
    TIMEOUT,
    PANIC,
    USER_TERMINATED,
    UNKNOWN
};

inline std::string ToString(ExitReason reason) {
    switch (reason) {
        case ExitReason::SUCCESS:         return "Success";
        case ExitReason::CRASH:           return "Crash";
        case ExitReason::TIMEOUT:         return "Timeout";
        case ExitReason::PANIC:           return "KernelPanic";
        case ExitReason::USER_TERMINATED: return "UserTerminated";
        default:                          return "Unknown";
    }
}

struct SessionManifest {
    std::string session_id;
    std::string start_time;
    std::string end_time;
    std::string rom_path;
    std::string kernel_path;
    std::string dtb_path;
    ExitReason exit_reason{ExitReason::UNKNOWN};
    double total_duration_seconds{0.0};
    uint64_t total_events{0};

    nlohmann::json ToJson() const {
        return nlohmann::json{
            {"session_id", session_id},
            {"start_time", start_time},
            {"end_time", end_time},
            {"rom_path", rom_path},
            {"kernel_path", kernel_path},
            {"dtb_path", dtb_path},
            {"exit_reason", ToString(exit_reason)},
            {"total_duration_seconds", total_duration_seconds},
            {"total_events", total_events}
        };
    }
};

} // namespace emulator::logger::models
