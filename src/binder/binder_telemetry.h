#pragma once
#include "binder_transaction_record.h"
#include "binder_stats.h"
#include <vector>
#include <deque>
#include <mutex>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace emulator::binder {

class BinderTelemetry {
public:
    static BinderTelemetry& Instance();

    void SetHistoryDepth(size_t depth);
    size_t GetHistoryDepth() const { return max_history_depth_; }

    void LogIoctl(uint32_t cmd);
    void LogTransaction(BinderTransactionRecord tx);

    std::vector<BinderTransactionRecord> GetHistory() const;
    void Clear();

    BinderStats& GetStats() { return stats_; }
    const BinderStats& GetStats() const { return stats_; }

    nlohmann::json ToJson() const;
    void SaveHistoryJson(const std::filesystem::path& file_path) const;

private:
    BinderTelemetry();
    ~BinderTelemetry() = default;

    BinderTelemetry(const BinderTelemetry&) = delete;
    BinderTelemetry& operator=(const BinderTelemetry&) = delete;

    mutable std::mutex mutex_;
    size_t max_history_depth_{64};
    std::deque<BinderTransactionRecord> history_;
    BinderStats stats_;
};

} // namespace emulator::binder
