#include "behavior_exploration_tool/experiment_database.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>
#include <filesystem>

namespace bet {

using json = nlohmann::json;

ExperimentDatabase::ExperimentDatabase(const std::string& dbFilePath)
    : m_dbFilePath(dbFilePath) {}

bool ExperimentDatabase::load() {
    m_runs.clear();
    if (!std::filesystem::exists(m_dbFilePath)) {
        return true; // DB starts empty
    }
    
    try {
        std::ifstream in(m_dbFilePath);
        if (!in.is_open()) return false;
        
        json jArray;
        in >> jArray;
        in.close();
        
        for (const auto& j : jArray) {
            ExperimentRun run;
            run.bootId = j.value("bootId", 0);
            run.timestamp = j.value("timestamp", "");
            run.gitCommit = j.value("gitCommit", "");
            run.emulatorVersion = j.value("emulatorVersion", "");
            run.romName = j.value("romName", "");
            run.kernelVersion = j.value("kernelVersion", "");
            
            // Progress Report
            if (j.count("progress")) {
                const auto& jp = j.at("progress");
                run.progress.durationSeconds = jp.value("durationSeconds", 0.0);
                run.progress.totalSmcCount = jp.value("totalSmcCount", 0);
                run.progress.uniqueSmcCount = jp.value("uniqueSmcCount", 0);
                
                std::string lastPcStr = jp.value("lastPc", "0x0");
                try {
                    run.progress.lastPc = std::stoull(lastPcStr, nullptr, 16);
                } catch (...) {
                    run.progress.lastPc = 0;
                }
                
                run.progress.lastSymbol = jp.value("lastSymbol", "");
                run.progress.lastSerialLine = jp.value("lastSerialLine", "");
                run.progress.initializedDriversCount = jp.value("initializedDriversCount", 0);
                run.progress.initializedDrivers = jp.value("initializedDrivers", std::vector<std::string>{});
            }
            
            // Timeline
            if (j.count("timeline")) {
                for (const auto& jt : j.at("timeline")) {
                    TimelineEvent te;
                    te.elapsedSeconds = jt.value("elapsedSeconds", 0.0);
                    te.timestamp = jt.value("timestamp", "");
                    te.category = jt.value("category", "");
                    te.description = jt.value("description", "");
                    run.timeline.push_back(te);
                }
            }
            
            run.reportMarkdown = j.value("reportMarkdown", "");
            run.eventsFilePath = j.value("eventsFilePath", "");
            run.serialLogFilePath = j.value("serialLogFilePath", "");
            
            m_runs.push_back(run);
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[BET] ExperimentDatabase: Failed to load DB JSON: {}", e.what());
        return false;
    }
}

bool ExperimentDatabase::save() {
    try {
        json jArray = json::array();
        for (const auto& run : m_runs) {
            json j;
            j["bootId"] = run.bootId;
            j["timestamp"] = run.timestamp;
            j["gitCommit"] = run.gitCommit;
            j["emulatorVersion"] = run.emulatorVersion;
            j["romName"] = run.romName;
            j["kernelVersion"] = run.kernelVersion;
            
            // Progress
            json jp;
            jp["durationSeconds"] = run.progress.durationSeconds;
            jp["totalSmcCount"] = run.progress.totalSmcCount;
            jp["uniqueSmcCount"] = run.progress.uniqueSmcCount;
            jp["lastPc"] = fmt::format("0x{:016x}", run.progress.lastPc);
            jp["lastSymbol"] = run.progress.lastSymbol;
            jp["lastSerialLine"] = run.progress.lastSerialLine;
            jp["initializedDriversCount"] = run.progress.initializedDriversCount;
            jp["initializedDrivers"] = run.progress.initializedDrivers;
            j["progress"] = jp;
            
            // Timeline
            json jtArray = json::array();
            for (const auto& te : run.timeline) {
                json jt;
                jt["elapsedSeconds"] = te.elapsedSeconds;
                jt["timestamp"] = te.timestamp;
                jt["category"] = te.category;
                jt["description"] = te.description;
                jtArray.push_back(jt);
            }
            j["timeline"] = jtArray;
            
            j["reportMarkdown"] = run.reportMarkdown;
            j["eventsFilePath"] = run.eventsFilePath;
            j["serialLogFilePath"] = run.serialLogFilePath;
            
            jArray.push_back(j);
        }
        
        // Ensure directory exists
        std::filesystem::path dbPath(m_dbFilePath);
        std::filesystem::path parentDir = dbPath.parent_path();
        if (!parentDir.empty()) {
            std::filesystem::create_directories(parentDir);
        }
        
        std::ofstream out(m_dbFilePath);
        if (!out.is_open()) return false;
        
        out << jArray.dump(4);
        out.close();
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[BET] ExperimentDatabase: Failed to save DB JSON: {}", e.what());
        return false;
    }
}

void ExperimentDatabase::addRun(const ExperimentRun& run) {
    // Check if run already exists, replace it
    auto it = std::find_if(m_runs.begin(), m_runs.end(), [&run](const ExperimentRun& r) {
        return r.bootId == run.bootId;
    });
    
    if (it != m_runs.end()) {
        *it = run;
    } else {
        m_runs.push_back(run);
    }
}

bool ExperimentDatabase::getRun(int bootId, ExperimentRun& outRun) const {
    auto it = std::find_if(m_runs.begin(), m_runs.end(), [bootId](const ExperimentRun& r) {
        return r.bootId == bootId;
    });
    if (it != m_runs.end()) {
        outRun = *it;
        return true;
    }
    return false;
}

bool ExperimentDatabase::getLatestRunForRom(const std::string& romName, ExperimentRun& outRun) const {
    ExperimentRun latest;
    bool found = false;
    for (const auto& run : m_runs) {
        if (run.romName == romName) {
            if (!found || run.bootId > latest.bootId) {
                latest = run;
                found = true;
            }
        }
    }
    if (found) {
        outRun = latest;
        return true;
    }
    return false;
}

int ExperimentDatabase::getNextBootId() const {
    int maxId = 0;
    for (const auto& run : m_runs) {
        if (run.bootId > maxId) {
            maxId = run.bootId;
        }
    }
    return maxId + 1;
}

} // namespace bet
