#pragma once
#include <string_view>

namespace emulator::logger::models {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
    HIT,
    PANIC,
    PHASE_BEGIN,
    PHASE_END,
    ANOMALY
};

inline std::string_view ToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:       return "DEBUG";
        case LogLevel::INFO:        return "INFO";
        case LogLevel::WARN:        return "WARN";
        case LogLevel::ERROR:       return "ERROR";
        case LogLevel::FATAL:       return "FATAL";
        case LogLevel::HIT:         return "HIT";
        case LogLevel::PANIC:       return "PANIC";
        case LogLevel::PHASE_BEGIN: return "PHASE BEGIN";
        case LogLevel::PHASE_END:   return "PHASE END";
        case LogLevel::ANOMALY:     return "ANOMALY";
        default:                    return "INFO";
    }
}

} // namespace emulator::logger::models
