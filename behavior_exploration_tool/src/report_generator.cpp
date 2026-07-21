#include "behavior_exploration_tool/report_generator.h"
#include <sstream>
#include <iomanip>

namespace bet {

std::string ReportGenerator::generateMarkdownReport(
    int bootId,
    const std::string& romName,
    const std::string& gitCommit,
    const std::string& emulatorVersion,
    const ProgressReport& progress,
    const std::vector<BlockerObservation>& observations,
    const ComparisonReport& comparison) const {
    
    std::stringstream ss;
    
    ss << "# Behavior Exploration Tool (BET) Boot Report\n\n";
    
    // Metadata block
    ss << "## Session Metadata\n";
    ss << "* **Boot ID**: " << bootId << "\n";
    ss << "* **ROM**: " << romName << "\n";
    ss << "* **Git Commit**: `" << gitCommit << "`\n";
    ss << "* **Emulator Version**: " << emulatorVersion << "\n";
    ss << "* **Total Duration**: " << std::fixed << std::setprecision(2) << progress.durationSeconds << " seconds\n\n";
    
    // Last Successful Progress
    ss << "## Last Successful Execution Point\n";
    if (progress.lastPc != 0) {
        ss << "* **PC**: `0x" << std::hex << progress.lastPc << std::dec << "`\n";
        if (!progress.lastSymbol.empty()) {
            ss << "* **Nearest Symbol**: `" << progress.lastSymbol << "`\n";
        } else {
            ss << "* **Nearest Symbol**: `[Unknown / Missing Symbol Map]`\n";
        }
    } else {
        ss << "* No guest code execution was successfully intercepted.\n";
    }
    ss << "\n";
    
    // First Observed Blocking Event
    ss << "## First Observed Blocking Event\n";
    if (!observations.empty()) {
        const auto& obs = observations.front();
        ss << "* **Type**: " << obs.type << "\n";
        ss << "* **Description**: " << obs.description << "\n";
        ss << "* **Evidence**: `" << obs.evidence << "`\n";
        
        if (observations.size() > 1) {
            ss << "\n### Additional Blocker Observations\n";
            for (size_t i = 1; i < observations.size(); ++i) {
                ss << "* **" << observations[i].type << "**: " << observations[i].description << "\n";
            }
        }
    } else {
        ss << "* No blocking pattern (infinite loop, watchdog timeout, or panic) was explicitly observed.\n";
    }
    ss << "\n";
    
    // Progress Metrics
    ss << "## Boot Progress Metrics\n";
    ss << "* **Total SMC Intercepts**: " << progress.totalSmcCount << "\n";
    ss << "* **Unique SMC Commands Query**: " << progress.uniqueSmcCount << "\n";
    ss << "* **Initialized Driver Modules**: " << progress.initializedDriversCount << "\n";
    if (!progress.initializedDrivers.empty()) {
        ss << "  * Initialized Drivers List: ";
        for (size_t i = 0; i < progress.initializedDrivers.size(); ++i) {
            ss << "`" << progress.initializedDrivers[i] << "`" << (i + 1 < progress.initializedDrivers.size() ? ", " : "");
        }
        ss << "\n";
    }
    ss << "\n";
    
    // Comparison Engine Results
    ss << "## Comparison Summary\n";
    if (comparison.hasPreviousRun) {
        ss << "* **Compare Status**: " << comparison.progressStatus << "\n";
        ss << "* **SMC Count Delta**: " << (comparison.smcCountDelta >= 0 ? "+" : "") << comparison.smcCountDelta << "\n";
        ss << "* **Driver Count Delta**: " << (comparison.driverCountDelta >= 0 ? "+" : "") << comparison.driverCountDelta << "\n";
        if (comparison.reachedNewPc) {
            ss << "* Reached new virtual machine Program Counter (PC).\n";
        }
        if (!comparison.newDrivers.empty()) {
            ss << "* **New Drivers Detected**:\n";
            for (const auto& d : comparison.newDrivers) {
                ss << "  * `" << d << "`\n";
            }
        }
    } else {
        ss << "* First run recorded for this ROM package configurations. No previous baseline comparison exists.\n";
    }
    ss << "\n";
    
    return ss.str();
}

} // namespace bet
