#include "behavior_exploration_tool/progress_analyzer.h"
#include <set>
#include <algorithm>

namespace bet {

void ProgressAnalyzer::clear() {
    m_report = ProgressReport();
}

void ProgressAnalyzer::analyzeProgress(const std::vector<BetEvent>& events) {
    clear();
    
    std::set<std::string> uniqueSmcs;
    std::vector<std::string> drivers;
    
    double maxTime = 0.0;
    
    for (const auto& ev : events) {
        if (ev.elapsedSeconds > maxTime) {
            maxTime = ev.elapsedSeconds;
        }

        if (ev.type == BetEventType::BootStart) {
            continue;
        }

        if (ev.type == BetEventType::BootEnd) {
            continue;
        }

        if (ev.type == BetEventType::GuestIntercept) {
            if (ev.category == "SMC") {
                m_report.totalSmcCount++;
                uniqueSmcs.insert(ev.name);
            }
            if (ev.pc != 0) {
                m_report.lastPc = ev.pc;
                if (ev.metadata.count("symbol")) {
                    m_report.lastSymbol = ev.metadata.at("symbol");
                }
            }
        } else if (ev.type == BetEventType::SerialLogLine) {
            m_report.lastSerialLine = ev.details;
            
            // Look for successful driver initialization messages
            // E.g.: "probe of xxx succeeded" or "xxx: probe succeeded"
            if (ev.details.find("probe of") != std::string::npos && 
                ev.details.find("succeeded") != std::string::npos) {
                
                size_t startPos = ev.details.find("probe of") + 9;
                size_t endPos = ev.details.find("succeeded") - 1;
                if (endPos > startPos) {
                    std::string driverName = ev.details.substr(startPos, endPos - startPos + 1);
                    // Trim spaces
                    driverName.erase(0, driverName.find_first_not_of(" \t\r\n"));
                    driverName.erase(driverName.find_last_not_of(" \t\r\n") + 1);
                    
                    if (std::find(drivers.begin(), drivers.end(), driverName) == drivers.end()) {
                        drivers.push_back(driverName);
                    }
                }
            } else if (ev.details.find("UFS") != std::string::npos && 
                       ev.details.find("probe succeeded") != std::string::npos) {
                std::string driverName = "ufs_qcom";
                if (std::find(drivers.begin(), drivers.end(), driverName) == drivers.end()) {
                    drivers.push_back(driverName);
                }
            }
        }
    }
    
    m_report.durationSeconds = maxTime;
    m_report.uniqueSmcCount = static_cast<int>(uniqueSmcs.size());
    m_report.initializedDrivers = drivers;
    m_report.initializedDriversCount = static_cast<int>(drivers.size());
}

} // namespace bet
