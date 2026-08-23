#pragma once
#include <cstdint>
#include <string>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

namespace emulator::binder {

class BinderStats {
public:
    BinderStats() = default;

    void RecordTransaction(const std::string& iface, double latency_ms, bool success);
    void Reset();

    uint64_t GetTotalTransactions() const { return total_tx_; }
    uint64_t GetFailedTransactions() const { return failed_tx_; }
    double GetAverageLatencyMs() const;

    uint64_t GetSurfaceComposerTx() const { return surfacecomposer_tx_; }
    uint64_t GetActivityManagerTx() const { return activitymanager_tx_; }
    uint64_t GetWindowManagerTx() const { return windowmanager_tx_; }

    nlohmann::json ToJson() const;

private:
    mutable std::mutex mutex_;
    uint64_t total_tx_{0};
    uint64_t failed_tx_{0};
    double total_latency_ms_{0.0};

    uint64_t surfacecomposer_tx_{0};
    uint64_t activitymanager_tx_{0};
    uint64_t windowmanager_tx_{0};
    uint64_t servicemanager_tx_{0};
    uint64_t other_tx_{0};

    std::map<std::string, uint64_t> per_interface_counts_;
};

} // namespace emulator::binder
