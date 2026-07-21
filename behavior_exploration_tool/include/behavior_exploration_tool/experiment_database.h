#pragma once

#include "bet_types.h"
#include <vector>
#include <string>
#include <map>

namespace bet {

struct ExperimentRun {
    int bootId = 0;
    std::string timestamp;
    std::string gitCommit;
    std::string emulatorVersion;
    std::string romName;
    std::string kernelVersion;
    
    ProgressReport progress;
    std::vector<TimelineEvent> timeline;
    std::string reportMarkdown;
    std::string eventsFilePath;
    std::string serialLogFilePath;
};

class ExperimentDatabase {
public:
    explicit ExperimentDatabase(const std::string& dbFilePath);
    ~ExperimentDatabase() = default;

    // Load database from file
    bool load();
    
    // Save database to file
    bool save();

    // Add a run record
    void addRun(const ExperimentRun& run);

    // Get a historical run
    bool getRun(int bootId, ExperimentRun& outRun) const;
    bool getLatestRunForRom(const std::string& romName, ExperimentRun& outRun) const;
    
    int getNextBootId() const;
    
    const std::vector<ExperimentRun>& getAllRuns() const { return m_runs; }

private:
    std::string m_dbFilePath;
    std::vector<ExperimentRun> m_runs;
};

} // namespace bet
