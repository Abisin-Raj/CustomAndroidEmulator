#include "behavior_exploration_tool/comparison_engine.h"
#include <sstream>
#include <algorithm>

namespace bet {

ComparisonReport ComparisonEngine::compareRuns(const ProgressReport& current, const ProgressReport& previous) {
    ComparisonReport report;
    report.hasPreviousRun = true;
    
    report.smcCountDelta = current.totalSmcCount - previous.totalSmcCount;
    report.driverCountDelta = current.initializedDriversCount - previous.initializedDriversCount;
    
    // Find new drivers
    for (const auto& d : current.initializedDrivers) {
        if (std::find(previous.initializedDrivers.begin(), previous.initializedDrivers.end(), d) == previous.initializedDrivers.end()) {
            report.newDrivers.push_back(d);
        }
    }
    
    // PC progress check
    if (current.lastPc != previous.lastPc) {
        report.reachedNewPc = true;
    }
    
    // Determine progress status
    if (report.driverCountDelta > 0 || (!report.newDrivers.empty()) || report.smcCountDelta > 0) {
        report.progressStatus = "Progressed";
    } else if (report.driverCountDelta < 0 || report.smcCountDelta < 0) {
        report.progressStatus = "Regressed";
    } else {
        report.progressStatus = "No Change";
    }
    
    return report;
}

std::string ComparisonEngine::generateComparisonSummary(const ComparisonReport& report) const {
    if (!report.hasPreviousRun) {
        return "No historical run available for comparison.";
    }

    std::stringstream ss;
    ss << "Comparison with previous run:\n";
    ss << "  Status: " << report.progressStatus << "\n";
    ss << "  SMC Delta: " << (report.smcCountDelta >= 0 ? "+" : "") << report.smcCountDelta << "\n";
    ss << "  Driver Delta: " << (report.driverCountDelta >= 0 ? "+" : "") << report.driverCountDelta << "\n";
    if (!report.newDrivers.empty()) {
        ss << "  New Drivers Initialized:\n";
        for (const auto& d : report.newDrivers) {
            ss << "    - " << d << "\n";
        }
    }
    return ss.str();
}

} // namespace bet
