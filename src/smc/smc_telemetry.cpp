#include "smc_telemetry.h"
#include "src/logger/api/unified_logger.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace emulator::smc {

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

SmcTelemetry& SmcTelemetry::Instance() {
    static SmcTelemetry instance;
    return instance;
}

SmcTelemetry::SmcTelemetry()
    : max_history_depth_(32) {}

void SmcTelemetry::SetHistoryDepth(size_t depth) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_history_depth_ = depth;
    while (history_.size() > max_history_depth_) {
        history_.pop_front();
    }
}

void SmcTelemetry::LogCall(SmcCallRecord record) {
    if (record.service_name.empty() || record.function_name.empty()) {
        SmcCallRecord::ResolveOpcodeNames(record.opcode, record.service_name, record.function_name);
    }
    if (record.timestamp.empty()) {
        record.timestamp = GetCurrentTimestamp();
    }

    std::optional<SmcRetryAnomaly> retry_anomaly;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        total_calls_++;
        total_latency_us_ += record.latency_us;
        opcode_counts_[record.opcode]++;
        if (record.ret_x0 != 0) {
            total_failures_++;
        }

        if (history_.size() >= max_history_depth_) {
            history_.pop_front();
        }
        history_.push_back(record);

        retry_anomaly = retry_detector_.Track(record);
    }

    // Emit structured CALL log
    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::SMC,
        ::emulator::logger::models::LogLevel::INFO,
        record.FormatCallLog(),
        record.pc);

    // Emit structured RETURN log
    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::SMC,
        ::emulator::logger::models::LogLevel::INFO,
        record.FormatReturnLog(),
        record.pc,
        record.latency_us / 1000.0);

    // If retry anomaly detected, emit warning
    if (retry_anomaly.has_value()) {
        std::ostringstream ss;
        ss << "SMC RETRY LOOP: Opcode 0x" << std::hex << retry_anomaly->opcode << std::dec
           << " [" << retry_anomaly->opcode_name << "] repeated "
           << retry_anomaly->count << " times (First PC: 0x"
           << std::hex << retry_anomaly->first_pc << std::dec << ")";
        
        ::emulator::logger::api::UnifiedLogger::Instance().Log(
            ::emulator::logger::models::Subsystem::SMC,
            ::emulator::logger::models::LogLevel::WARN,
            ss.str(),
            retry_anomaly->first_pc);
    }
}

std::vector<SmcCallRecord> SmcTelemetry::GetHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<SmcCallRecord>(history_.begin(), history_.end());
}

void SmcTelemetry::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.clear();
    retry_detector_.Reset();
    total_calls_ = 0;
    total_failures_ = 0;
    total_latency_us_ = 0.0;
    opcode_counts_.clear();
}

const std::map<uint32_t, uint64_t>& SmcTelemetry::GetOpcodeCounts() const {
    return opcode_counts_;
}

double SmcTelemetry::GetAverageLatencyUs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (total_calls_ == 0) return 0.0;
    return total_latency_us_ / static_cast<double>(total_calls_);
}

nlohmann::json SmcTelemetry::ToJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j = nlohmann::json::array();
    for (const auto& call : history_) {
        j.push_back(call.ToJson());
    }
    return j;
}

void SmcTelemetry::SaveHistoryJson(const std::filesystem::path& file_path) const {
    if (file_path.has_parent_path()) {
        std::filesystem::create_directories(file_path.parent_path());
    }
    std::ofstream ofs(file_path);
    if (ofs.is_open()) {
        ofs << ToJson().dump(2);
    }
}

} // namespace emulator::smc
