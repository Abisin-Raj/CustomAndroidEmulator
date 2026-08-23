#pragma once
#include "subsystem.h"
#include "log_level.h"
#include <chrono>
#include <string>
#include <optional>
#include <cstdint>

namespace emulator::logger::models {

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    Subsystem subsystem{Subsystem::UNKNOWN};
    LogLevel level{LogLevel::INFO};
    std::string message;
    
    // Optional diagnostics metadata
    std::optional<uint64_t> pc;
    std::optional<uint32_t> syscall_nr;
    std::optional<double> duration_ms;
    std::optional<uint32_t> thread_id;
};

} // namespace emulator::logger::models
