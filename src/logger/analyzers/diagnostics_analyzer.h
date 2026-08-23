#pragma once
#include "../models/log_entry.h"
#include "../models/metrics.h"
#include "../models/session_manifest.h"
#include <vector>
#include <string>
#include <filesystem>
#include <optional>
#include <map>

namespace emulator::logger::analyzers {

struct Anomaly {
    std::string type;
    std::string subsystem;
    std::optional<uint64_t> pc;
    std::string details;
    std::string likely_cause;
    std::vector<std::string> last_instructions;
};


struct DiagnosticsReport {
    models::ExitReason exit_reason{models::ExitReason::UNKNOWN};
    std::string summary;
    std::vector<Anomaly> detected_anomalies;
    std::vector<models::LogEntry> failure_context_100_events;
    std::optional<uint64_t> last_executed_pc;
    models::Metrics computed_metrics;
};

class DiagnosticsAnalyzer {
public:
    DiagnosticsAnalyzer() = default;

    DiagnosticsReport Analyze(const std::vector<models::LogEntry>& full_history,
                             models::ExitReason proposed_exit_reason);

    void WriteReport(const DiagnosticsReport& report,
                     const std::filesystem::path& session_dir);

private:
    void DetectPanics(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const;
    void DetectInfiniteLoops(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const;
    void DetectDeadlocksAndStalls(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const;
    void DetectMountFailures(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const;
    void DetectSmcRetryLoops(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const;
    void DetectBinderStalls(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const;
    void DetectUfsTimeouts(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const;
    void DetectFrameTimeouts(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const;
    void ComputePhasesAndMetrics(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const;
};

} // namespace emulator::logger::analyzers
