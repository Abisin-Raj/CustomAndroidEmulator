#include "binder_stats.h"

namespace emulator::binder {

void BinderStats::RecordTransaction(const std::string& iface, double latency_ms, bool success) {
    std::lock_guard<std::mutex> lock(mutex_);
    total_tx_++;
    total_latency_ms_ += latency_ms;
    if (!success) {
        failed_tx_++;
    }

    per_interface_counts_[iface]++;

    if (iface.find("SurfaceComposer") != std::string::npos || iface.find("ISurfaceComposer") != std::string::npos) {
        surfacecomposer_tx_++;
    } else if (iface.find("ActivityManager") != std::string::npos || iface.find("IActivityManager") != std::string::npos) {
        activitymanager_tx_++;
    } else if (iface.find("WindowManager") != std::string::npos || iface.find("IWindowManager") != std::string::npos) {
        windowmanager_tx_++;
    } else if (iface.find("ServiceManager") != std::string::npos || iface.find("IServiceManager") != std::string::npos) {
        servicemanager_tx_++;
    } else {
        other_tx_++;
    }
}

void BinderStats::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    total_tx_ = 0;
    failed_tx_ = 0;
    total_latency_ms_ = 0.0;
    surfacecomposer_tx_ = 0;
    activitymanager_tx_ = 0;
    windowmanager_tx_ = 0;
    servicemanager_tx_ = 0;
    other_tx_ = 0;
    per_interface_counts_.clear();
}

double BinderStats::GetAverageLatencyMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (total_tx_ == 0) return 0.0;
    return total_latency_ms_ / static_cast<double>(total_tx_);
}

nlohmann::json BinderStats::ToJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j;
    j["binder_transactions"] = total_tx_;
    j["binder_failed"] = failed_tx_;
    j["avg_binder_latency_ms"] = total_tx_ > 0 ? (total_latency_ms_ / total_tx_) : 0.0;
    j["surfacecomposer_tx"] = surfacecomposer_tx_;
    j["activitymanager_tx"] = activitymanager_tx_;
    j["windowmanager_tx"] = windowmanager_tx_;
    j["per_interface_counts"] = per_interface_counts_;
    return j;
}

} // namespace emulator::binder
