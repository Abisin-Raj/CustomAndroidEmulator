#include "input_telemetry.h"
#include "src/logger/api/unified_logger.h"
#include <fstream>
#include <sstream>

namespace emulator::input {

InputTelemetry& InputTelemetry::Instance() {
    static InputTelemetry instance;
    return instance;
}

void InputTelemetry::LogEvent(const InputEventRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    total_injected_++;

    if (history_.size() >= max_history_) {
        history_.pop_front();
    }
    history_.push_back(record);

    std::ostringstream ss;
    ss << record.type_name << " " << record.code_name << " value=" << record.value;
    if (!record.target_window.empty()) {
        ss << " -> [" << record.target_window << "]";
    }
    if (record.dispatch_latency_ms > 0.0) {
        ss << " (latency: " << record.dispatch_latency_ms << "ms)";
    }

    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::INPUT,
        ::emulator::logger::models::LogLevel::INFO,
        ss.str());
}

void InputTelemetry::LogDispatch(const std::string& target_window, double latency_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    total_dispatched_++;
    total_latency_ms_ += latency_ms;

    if (!history_.empty()) {
        history_.back().target_window = target_window;
        history_.back().dispatch_latency_ms = latency_ms;
        history_.back().status = "DISPATCHED";
    }

    std::ostringstream ss;
    ss << "Event dispatched to window: " << target_window << " (latency: " << latency_ms << "ms)";
    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::INPUT,
        ::emulator::logger::models::LogLevel::INFO,
        ss.str());
}

double InputTelemetry::GetAverageLatencyMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (total_dispatched_ == 0) return 0.0;
    return total_latency_ms_ / static_cast<double>(total_dispatched_);
}

std::vector<InputEventRecord> InputTelemetry::GetRecentEvents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<InputEventRecord>(history_.begin(), history_.end());
}

void InputTelemetry::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.clear();
    total_injected_ = 0;
    total_dispatched_ = 0;
    total_latency_ms_ = 0.0;
}

void InputTelemetry::SaveHistoryJson(const std::filesystem::path& file_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::create_directories(file_path.parent_path());

    nlohmann::json root = nlohmann::json::array();
    for (const auto& ev : history_) {
        root.push_back(ev.ToJson());
    }

    std::ofstream out(file_path);
    if (out.is_open()) {
        out << root.dump(2);
    }
}

} // namespace emulator::input
