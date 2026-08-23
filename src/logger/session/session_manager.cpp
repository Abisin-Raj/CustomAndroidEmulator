#include "session_manager.h"
#include <iomanip>
#include <sstream>
#include <fstream>

namespace emulator::logger::session {

SessionManager::SessionManager(std::filesystem::path root_log_dir)
    : root_log_dir_(std::move(root_log_dir)),
      logger_core_(std::make_shared<core::LoggerCore>()) {}

SessionManager::~SessionManager() {
    if (active_) {
        EndSession(models::ExitReason::USER_TERMINATED);
    }
}

std::string SessionManager::GenerateSessionId() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d_%H-%M-%S");
    return ss.str();
}

std::string SessionManager::StartSession(const std::string& rom_path,
                                         const std::string& kernel_path,
                                         const std::string& dtb_path) {
    if (active_) {
        EndSession(models::ExitReason::USER_TERMINATED);
    }

    start_time_point_ = std::chrono::system_clock::now();
    std::string session_id = GenerateSessionId();
    current_session_dir_ = root_log_dir_ / session_id;
    std::filesystem::create_directories(current_session_dir_);
    std::filesystem::create_directories(current_session_dir_ / "artifacts" / "registers");
    std::filesystem::create_directories(current_session_dir_ / "artifacts" / "memory");
    std::filesystem::create_directories(current_session_dir_ / "artifacts" / "screenshots");
    std::filesystem::create_directories(current_session_dir_ / "artifacts" / "traces");

    current_manifest_.session_id = session_id;


    current_manifest_.start_time = session_id;
    current_manifest_.rom_path = rom_path;
    current_manifest_.kernel_path = kernel_path;
    current_manifest_.dtb_path = dtb_path;
    current_manifest_.exit_reason = models::ExitReason::UNKNOWN;
    current_manifest_.total_duration_seconds = 0.0;
    current_manifest_.total_events = 0;

    logger_core_->Initialize(current_session_dir_);
    active_ = true;

    // Log first boot event
    models::LogEntry entry;
    entry.timestamp = start_time_point_;
    entry.subsystem = models::Subsystem::BOOT;
    entry.level = models::LogLevel::INFO;
    entry.message = "Session started (ID: " + session_id + ")";
    logger_core_->Log(entry);

    return session_id;
}

void SessionManager::EndSession(models::ExitReason reason) {
    if (!active_) return;

    auto end_time_point = std::chrono::system_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time_point - start_time_point_).count();

    current_manifest_.exit_reason = reason;
    current_manifest_.total_duration_seconds = static_cast<double>(total_ms) / 1000.0;

    // Log session close event
    models::LogEntry close_entry;
    close_entry.timestamp = end_time_point;
    close_entry.subsystem = models::Subsystem::BOOT;
    close_entry.level = models::LogLevel::INFO;
    close_entry.message = "Session ended with reason: " + models::ToString(reason);
    logger_core_->Log(close_entry);

    // Flush and stop logger core
    logger_core_->Shutdown();

    // Fetch full history and execute diagnostics analysis
    auto history = logger_core_->GetRecentHistory(500);
    current_manifest_.total_events = history.size();

    auto report = diagnostics_analyzer_.Analyze(history, reason);
    diagnostics_analyzer_.WriteReport(report, current_session_dir_);

    ExportManifest();
    ExportMetrics(report.computed_metrics);

    active_ = false;
}

bool SessionManager::IsActive() const {
    return active_;
}

const std::filesystem::path& SessionManager::GetCurrentSessionDir() const {
    return current_session_dir_;
}

const models::SessionManifest& SessionManager::GetManifest() const {
    return current_manifest_;
}

std::shared_ptr<core::LoggerCore> SessionManager::GetLoggerCore() const {
    return logger_core_;
}

void SessionManager::ExportManifest() {
    auto manifest_path = current_session_dir_ / "manifest.json";
    std::ofstream ofs(manifest_path);
    if (ofs.is_open()) {
        ofs << current_manifest_.ToJson().dump(4);
    }
}

void SessionManager::ExportMetrics(const models::Metrics& metrics) {
    auto metrics_path = current_session_dir_ / "metrics.json";
    std::ofstream ofs(metrics_path);
    if (ofs.is_open()) {
        ofs << metrics.ToJson().dump(4);
    }
}

} // namespace emulator::logger::session
