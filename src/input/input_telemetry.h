#pragma once
#include "input_event_record.h"
#include <deque>
#include <mutex>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace emulator::input {

class InputTelemetry {
public:
    static InputTelemetry& Instance();

    void LogEvent(const InputEventRecord& record);
    void LogDispatch(const std::string& target_window, double latency_ms);
    void SaveHistoryJson(const std::filesystem::path& file_path) const;

    std::vector<InputEventRecord> GetRecentEvents() const;
    void Clear();

    uint64_t GetTotalEventsInjected() const { return total_injected_; }
    uint64_t GetTotalEventsDispatched() const { return total_dispatched_; }
    double GetAverageLatencyMs() const;

private:
    InputTelemetry() = default;
    ~InputTelemetry() = default;

    mutable std::mutex mutex_;
    std::deque<InputEventRecord> history_;
    size_t max_history_{64};

    uint64_t total_injected_{0};
    uint64_t total_dispatched_{0};
    double total_latency_ms_{0.0};
};

} // namespace emulator::input
