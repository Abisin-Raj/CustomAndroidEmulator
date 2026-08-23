#pragma once
#include "smc_call_record.h"
#include "smc_retry_detector.h"
#include <vector>
#include <deque>
#include <map>
#include <mutex>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace emulator::smc {

class SmcTelemetry {
public:
    static SmcTelemetry& Instance();

    void SetHistoryDepth(size_t depth);
    size_t GetHistoryDepth() const { return max_history_depth_; }

    void LogCall(SmcCallRecord record);

    std::vector<SmcCallRecord> GetHistory() const;
    void Clear();

    const std::map<uint32_t, uint64_t>& GetOpcodeCounts() const;
    uint64_t GetTotalCalls() const { return total_calls_; }
    uint64_t GetTotalFailures() const { return total_failures_; }
    double GetAverageLatencyUs() const;

    nlohmann::json ToJson() const;
    void SaveHistoryJson(const std::filesystem::path& file_path) const;

private:
    SmcTelemetry();
    ~SmcTelemetry() = default;

    SmcTelemetry(const SmcTelemetry&) = delete;
    SmcTelemetry& operator=(const SmcTelemetry&) = delete;

    mutable std::mutex mutex_;
    size_t max_history_depth_{32};
    std::deque<SmcCallRecord> history_;
    SmcRetryDetector retry_detector_;

    uint64_t total_calls_{0};
    uint64_t total_failures_{0};
    double total_latency_us_{0.0};
    std::map<uint32_t, uint64_t> opcode_counts_;
};

} // namespace emulator::smc
