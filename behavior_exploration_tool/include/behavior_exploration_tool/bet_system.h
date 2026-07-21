#pragma once

#include "bet_types.h"
#include "event_recorder.h"
#include "boot_timeline.h"
#include "progress_analyzer.h"
#include "comparison_engine.h"
#include "blocker_detector.h"
#include "report_generator.h"
#include "experiment_database.h"
#include "override_engine.h"
#include <memory>
#include <string>
#include <mutex>

namespace bet {

class BetSystem {
public:
    static BetSystem& instance();

    // Setup the database file path (e.g. at app start)
    void initialize(const std::string& dbDirectory);

    // Emergency switch check
    bool isOverridesEnabled() const;

    // Override engine interface
    bool checkOverride(const std::string& category, const std::string& name, 
                       std::string& outRuleId, std::string& outDesc, 
                       std::map<std::string, uint64_t>& outOverrides) const;

    // Session control
    void startSession(const std::string& romName, const std::string& kernelVersion, const std::string& emulatorVersion);
    void registerIntercept(const std::string& category, const std::string& name, uint64_t pc, uint64_t lr, uint64_t sp, 
                           const std::vector<uint64_t>& before, const std::vector<uint64_t>& dispatcher, const std::vector<uint64_t>& after, 
                           bool handled, const std::string& overrideRuleId = "",
                           const std::map<std::string, std::string>& metadata = {});
    void pushLogLine(const std::string& logLine);
    
    // Stop session, compute analysis, generate markdown report, save to database
    void endSession(const std::string& outcomeMessage);

    // Getters for active/completed report info
    int getActiveBootId() const { return m_activeBootId; }
    std::string getLatestReportMarkdown() const { return m_latestReportMarkdown; }
    
    // Database access
    ExperimentDatabase& getDatabase() { return *m_database; }

private:
    BetSystem() = default;
    ~BetSystem() = default;
    BetSystem(const BetSystem&) = delete;
    BetSystem& operator=(const BetSystem&) = delete;

    std::string getGitCommitHash() const;
    std::string getIsoTimestamp() const;

    std::unique_ptr<ExperimentDatabase> m_database;
    std::unique_ptr<EventRecorder> m_recorder;
    std::unique_ptr<BootTimeline> m_timeline;
    std::unique_ptr<ProgressAnalyzer> m_analyzer;
    std::unique_ptr<ComparisonEngine> m_comparator;
    std::unique_ptr<BlockerDetector> m_blockerDetector;
    std::unique_ptr<ReportGenerator> m_reportGenerator;
    std::unique_ptr<OverrideEngine> m_overrideEngine;

    std::string m_dbDir;
    bool m_initialized = false;
    int m_activeBootId = 0;
    std::string m_activeRomName;
    std::string m_activeKernelVersion;
    std::string m_activeEmulatorVersion;
    double m_sessionStartTime = 0.0;
    std::string m_latestReportMarkdown;
    
    mutable std::mutex m_mutex;
};

} // namespace bet
