#pragma once
#include "../models/session_manifest.h"
#include "../models/metrics.h"
#include "../core/logger_core.h"
#include "../analyzers/diagnostics_analyzer.h"
#include <string>
#include <filesystem>
#include <memory>
#include <chrono>

namespace emulator::logger::session {

class SessionManager {
public:
    explicit SessionManager(std::filesystem::path root_log_dir = "logger/sessions");
    ~SessionManager();

    std::string StartSession(const std::string& rom_path = "",
                             const std::string& kernel_path = "",
                             const std::string& dtb_path = "");

    void EndSession(models::ExitReason reason = models::ExitReason::SUCCESS);

    bool IsActive() const;
    const std::filesystem::path& GetCurrentSessionDir() const;
    const models::SessionManifest& GetManifest() const;
    
    std::shared_ptr<core::LoggerCore> GetLoggerCore() const;

private:
    std::string GenerateSessionId() const;
    void ExportManifest();
    void ExportMetrics(const models::Metrics& metrics);

    std::filesystem::path root_log_dir_;
    std::filesystem::path current_session_dir_;
    models::SessionManifest current_manifest_;
    std::chrono::system_clock::time_point start_time_point_;
    bool active_{false};

    std::shared_ptr<core::LoggerCore> logger_core_;
    analyzers::DiagnosticsAnalyzer diagnostics_analyzer_;
};

} // namespace emulator::logger::session
