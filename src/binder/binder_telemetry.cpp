#include "binder_telemetry.h"
#include "binder_ioctl_decoder.h"
#include "src/logger/api/unified_logger.h"
#include <fstream>
#include <sstream>
#include <chrono>

namespace emulator::binder {

static std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

BinderTelemetry& BinderTelemetry::Instance() {
    static BinderTelemetry instance;
    return instance;
}

BinderTelemetry::BinderTelemetry()
    : max_history_depth_(64) {}

void BinderTelemetry::SetHistoryDepth(size_t depth) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_history_depth_ = depth;
    while (history_.size() > max_history_depth_) {
        history_.pop_front();
    }
}

void BinderTelemetry::LogIoctl(uint32_t cmd) {
    std::string cmd_name = BinderIoctlDecoder::IoctlCmdToString(cmd);
    std::ostringstream ss;
    ss << "ioctl(" << cmd_name << ")";
    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::BINDER,
        ::emulator::logger::models::LogLevel::INFO,
        ss.str());
}

void BinderTelemetry::LogTransaction(BinderTransactionRecord tx) {
    if (tx.timestamp.empty()) {
        tx.timestamp = GetCurrentTimestamp();
    }
    if (tx.interface_name.empty()) {
        tx.interface_name = BinderTransactionRecord::ResolveInterface(tx.handle, tx.code);
    }

    bool success = (tx.result == "SUCCESS");
    stats_.RecordTransaction(tx.interface_name, tx.latency_ms, success);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (history_.size() >= max_history_depth_) {
            history_.pop_front();
        }
        history_.push_back(tx);
    }

    // 1. Log Command
    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::BINDER,
        ::emulator::logger::models::LogLevel::INFO,
        tx.FormatCommandLog());

    // 2. Log Reply Status & Latency
    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::BINDER,
        ::emulator::logger::models::LogLevel::INFO,
        tx.FormatReplyLog(), std::nullopt, tx.latency_ms);
}

std::vector<BinderTransactionRecord> BinderTelemetry::GetHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<BinderTransactionRecord>(history_.begin(), history_.end());
}

void BinderTelemetry::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.clear();
    stats_.Reset();
}

nlohmann::json BinderTelemetry::ToJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j = nlohmann::json::array();
    for (const auto& tx : history_) {
        j.push_back(tx.ToJson());
    }
    return j;
}

void BinderTelemetry::SaveHistoryJson(const std::filesystem::path& file_path) const {
    if (file_path.has_parent_path()) {
        std::filesystem::create_directories(file_path.parent_path());
    }
    std::ofstream ofs(file_path);
    if (ofs.is_open()) {
        ofs << ToJson().dump(2);
    }
}

} // namespace emulator::binder
