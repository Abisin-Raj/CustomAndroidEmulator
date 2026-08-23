#include "diagnostics_analyzer.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <map>

namespace emulator::logger::analyzers {

void DiagnosticsAnalyzer::DetectPanics(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const {
    for (const auto& entry : history) {
        std::string lower = entry.message;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.find("kernel panic") != std::string::npos ||
            lower.find("oops:") != std::string::npos ||
            lower.find("bug:") != std::string::npos ||
            lower.find("unhandled fault") != std::string::npos) {
            
            report.exit_reason = models::ExitReason::PANIC;
            Anomaly a;
            a.type = "Kernel Panic";
            a.subsystem = models::ToString(entry.subsystem);
            a.pc = entry.pc;
            a.details = entry.message;
            a.likely_cause = "Kernel fault or unhandled exception in driver/core";
            report.detected_anomalies.push_back(a);
        }
    }
}

void DiagnosticsAnalyzer::DetectInfiniteLoops(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const {
    std::map<uint64_t, int> pc_counts;
    for (const auto& entry : history) {
        if (entry.pc.has_value()) {
            pc_counts[entry.pc.value()]++;
        }
    }

    for (const auto& [pc, count] : pc_counts) {
        if (count >= 100) {
            Anomaly a;
            a.type = "Infinite Loop";
            a.subsystem = "GDB";
            a.pc = pc;
            std::ostringstream ss;
            ss << "Repeated: " << count << " executions";
            a.details = ss.str();
            a.likely_cause = "wait_for_device() / polling loop or spinlock without yield";

            // Find matching instructions
            for (const auto& entry : history) {
                if (entry.pc.has_value() && entry.pc.value() == pc) {
                    a.last_instructions.push_back(entry.message);
                    if (a.last_instructions.size() >= 4) break;
                }
            }

            report.detected_anomalies.push_back(a);
        }
    }

}

void DiagnosticsAnalyzer::DetectDeadlocksAndStalls(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const {
    for (const auto& entry : history) {
        if (entry.duration_ms.has_value() && entry.duration_ms.value() > 5000.0) {
            Anomaly a;
            a.type = "Soft Lockup / Deadlock";
            a.subsystem = models::ToString(entry.subsystem);
            a.pc = entry.pc;
            std::ostringstream ss;
            ss << entry.message << " stalled for " << std::fixed << std::setprecision(2) << entry.duration_ms.value() << "ms";
            a.details = ss.str();
            a.likely_cause = "Kernel scheduler stuck on task / deadlock";
            report.detected_anomalies.push_back(a);
        }
    }
}

void DiagnosticsAnalyzer::DetectMountFailures(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const {
    for (const auto& entry : history) {
        if (entry.subsystem == models::Subsystem::STORAGE &&
            (entry.message.find("ret=-") != std::string::npos || entry.message.find("FAILED") != std::string::npos)) {
            Anomaly a;
            a.type = "Mount Failure";
            a.subsystem = "STORAGE";
            a.pc = entry.pc;
            a.details = entry.message;
            a.likely_cause = "Ext4 superblock mismatch, missing block device node, or invalid flags";
            report.detected_anomalies.push_back(a);
        }
    }
}

void DiagnosticsAnalyzer::DetectSmcRetryLoops(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const {
    int smc_retry_count = 0;
    for (const auto& entry : history) {
        if (entry.subsystem == models::Subsystem::SMC && entry.message.find("RETRY") != std::string::npos) {
            smc_retry_count++;
        }
    }
    if (smc_retry_count >= 10) {
        Anomaly a;
        a.type = "SMC Retry Loop";
        a.subsystem = "SMC";
        std::ostringstream ss;
        ss << "Repeated SMC call retries: " << smc_retry_count;
        a.details = ss.str();
        a.likely_cause = "Qualcomm Secure Monitor Call returned unhandled error status";
        report.detected_anomalies.push_back(a);
    }
}

void DiagnosticsAnalyzer::DetectBinderStalls(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const {
    for (const auto& entry : history) {
        if (entry.subsystem == models::Subsystem::BINDER &&
            (entry.message.find("DEAD_OBJECT") != std::string::npos || entry.message.find("NO_REPLY") != std::string::npos)) {
            Anomaly a;
            a.type = "Binder Stall";
            a.subsystem = "BINDER";
            a.details = entry.message;
            a.likely_cause = "Target service died or did not respond to BC_TRANSACTION";
            report.detected_anomalies.push_back(a);
        }
    }
}

void DiagnosticsAnalyzer::DetectUfsTimeouts(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const {
    for (const auto& entry : history) {
        if (entry.subsystem == models::Subsystem::STORAGE &&
            entry.message.find("DOORBELL_TIMEOUT") != std::string::npos) {
            Anomaly a;
            a.type = "UFS Timeout";
            a.subsystem = "STORAGE";
            a.details = entry.message;
            a.likely_cause = "UFS controller doorbell bit never cleared / DMA stall";
            report.detected_anomalies.push_back(a);
        }
    }
}

void DiagnosticsAnalyzer::DetectFrameTimeouts(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const {
    for (const auto& entry : history) {
        if (entry.subsystem == models::Subsystem::DISPLAY &&
            entry.message.find("FRAME_TIMEOUT") != std::string::npos) {
            Anomaly a;
            a.type = "Frame Timeout";
            a.subsystem = "DISPLAY";
            a.details = entry.message;
            a.likely_cause = "SurfaceFlinger failed to produce new frame within display VSYNC window";
            report.detected_anomalies.push_back(a);
        }
    }
}

void DiagnosticsAnalyzer::ComputePhasesAndMetrics(const std::vector<models::LogEntry>& history, DiagnosticsReport& report) const {
    std::map<std::string, std::chrono::system_clock::time_point> phase_starts;

    for (const auto& entry : history) {
        if (entry.level == models::LogLevel::PHASE_BEGIN) {
            phase_starts[entry.message] = entry.timestamp;
        } else if (entry.level == models::LogLevel::PHASE_END) {
            auto it = phase_starts.find(entry.message);
            if (it != phase_starts.end()) {
                auto dur_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    entry.timestamp - it->second).count();
                
                models::PhaseDuration pd;
                pd.phase_name = entry.message;
                pd.duration_ms = static_cast<uint64_t>(dur_ms);
                pd.completed = true;
                report.computed_metrics.phase_map[entry.message] = pd;

                if (entry.message == "Kernel") report.computed_metrics.kernel_time_ms = pd.duration_ms;
                else if (entry.message == "FirstStageInit") report.computed_metrics.first_stage_ms = pd.duration_ms;
                else if (entry.message == "SecondStageInit") report.computed_metrics.second_stage_ms = pd.duration_ms;
                else if (entry.message == "Zygote") report.computed_metrics.zygote_ms = pd.duration_ms;
                else if (entry.message == "SurfaceFlinger") report.computed_metrics.surfaceflinger_ms = pd.duration_ms;
            }
        }

        if (entry.subsystem == models::Subsystem::BINDER) {
            report.computed_metrics.binder_transactions++;
        } else if (entry.subsystem == models::Subsystem::SMC) {
            report.computed_metrics.smc_calls++;
        } else if (entry.subsystem == models::Subsystem::STORAGE && entry.message.find("READ") != std::string::npos) {
            report.computed_metrics.ufs_reads++;
        }
    }
}

DiagnosticsReport DiagnosticsAnalyzer::Analyze(const std::vector<models::LogEntry>& full_history,
                                             models::ExitReason proposed_exit_reason) {
    DiagnosticsReport report;
    report.exit_reason = proposed_exit_reason;

    if (full_history.empty()) {
        report.summary = "No log events recorded during this session.";
        return report;
    }

    auto start_time = full_history.front().timestamp;
    auto end_time = full_history.back().timestamp;
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    report.computed_metrics.boot_time_ms = static_cast<uint64_t>(total_ms);

    size_t start_idx = full_history.size() > 100 ? full_history.size() - 100 : 0;
    report.failure_context_100_events.assign(full_history.begin() + start_idx, full_history.end());

    for (const auto& entry : full_history) {
        if (entry.pc.has_value()) report.last_executed_pc = entry.pc.value();
        if (entry.level == models::LogLevel::ERROR || entry.level == models::LogLevel::FATAL) report.computed_metrics.total_errors++;
        else if (entry.level == models::LogLevel::WARN) report.computed_metrics.total_warnings++;
        else if (entry.level == models::LogLevel::HIT) report.computed_metrics.total_gdb_hits++;
    }

    DetectPanics(full_history, report);
    DetectInfiniteLoops(full_history, report);
    DetectDeadlocksAndStalls(full_history, report);
    DetectMountFailures(full_history, report);
    DetectSmcRetryLoops(full_history, report);
    DetectBinderStalls(full_history, report);
    DetectUfsTimeouts(full_history, report);
    DetectFrameTimeouts(full_history, report);
    ComputePhasesAndMetrics(full_history, report);

    std::ostringstream summary_ss;
    summary_ss << "Session ended with status: " << models::ToString(report.exit_reason) << ". "
               << "Total Duration: " << total_ms << "ms. "
               << "Events Processed: " << full_history.size() << ". "
               << "Anomalies Flagged: " << report.detected_anomalies.size() << ".";
    report.summary = summary_ss.str();

    return report;
}

void DiagnosticsAnalyzer::WriteReport(const DiagnosticsReport& report,
                                      const std::filesystem::path& session_dir) {
    auto diag_path = session_dir / "diagnostics.log";
    std::ofstream ofs(diag_path);
    if (!ofs.is_open()) return;

    ofs << "================================================================================\n";
    ofs << "CUSTOM ANDROID EMULATOR - AUTOMATED DIAGNOSTICS & FAILURE ANALYSIS REPORT\n";
    ofs << "================================================================================\n\n";

    ofs << "[1] EXECUTIVE SUMMARY\n";
    ofs << "    Exit Reason:       " << models::ToString(report.exit_reason) << "\n";
    ofs << "    Summary:           " << report.summary << "\n";
    if (report.last_executed_pc.has_value()) {
        ofs << "    Last Executed PC:  0x" << std::hex << report.last_executed_pc.value() << std::dec << "\n";
    }
    ofs << "\n";

    ofs << "[2] DETECTED ANOMALIES & BOTTLENECKS (" << report.detected_anomalies.size() << ")\n";
    if (report.detected_anomalies.empty()) {
        ofs << "    No fatal bottlenecks, deadlocks, or kernel panics detected.\n";
    } else {
        for (size_t i = 0; i < report.detected_anomalies.size(); ++i) {
            const auto& a = report.detected_anomalies[i];
            ofs << "--------------------------------------------------------------------------------\n";
            ofs << "[ANOMALY " << (i + 1) << "]\n";
            ofs << "  Type:         " << a.type << "\n";
            ofs << "  Subsystem:    " << a.subsystem << "\n";
            if (a.pc.has_value()) {
                ofs << "  PC:           0x" << std::hex << a.pc.value() << std::dec << "\n";
            }
            ofs << "  Details:      " << a.details << "\n";
            if (!a.last_instructions.empty()) {
                ofs << "  Last Instructions:\n";
                for (size_t k = 0; k < a.last_instructions.size(); ++k) {
                    ofs << "    " << (k + 1) << ". " << a.last_instructions[k] << "\n";
                }
            }
            ofs << "  Likely Cause: " << a.likely_cause << "\n";
        }
        ofs << "--------------------------------------------------------------------------------\n";

    }
    ofs << "\n";

    ofs << "[3] LAST 100 RECORDED EVENTS BEFORE TERMINATION\n";
    ofs << "--------------------------------------------------------------------------------\n";
    for (const auto& entry : report.failure_context_100_events) {
        auto time_t_val = std::chrono::system_clock::to_time_t(entry.timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            entry.timestamp.time_since_epoch()) % 1000;

        std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_buf, &time_t_val);
#else
        localtime_r(&time_t_val, &tm_buf);
#endif

        ofs << std::put_time(&tm_buf, "%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << std::setfill(' ')
            << " [" << models::ToString(entry.subsystem) << "] "
            << std::left << std::setw(11) << models::ToString(entry.level)
            << " " << entry.message;

        if (entry.pc.has_value()) {
            ofs << " (PC=0x" << std::hex << entry.pc.value() << std::dec << ")";
        }
        ofs << "\n";
    }
    ofs << "================================================================================\n";
}

} // namespace emulator::logger::analyzers
