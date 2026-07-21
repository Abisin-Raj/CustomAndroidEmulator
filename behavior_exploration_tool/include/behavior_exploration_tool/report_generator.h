#pragma once

#include "bet_types.h"
#include "blocker_detector.h"
#include <string>
#include <vector>

namespace bet {

class ReportGenerator {
public:
    ReportGenerator() = default;
    ~ReportGenerator() = default;

    // Generate markdown report text
    std::string generateMarkdownReport(
        int bootId,
        const std::string& romName,
        const std::string& gitCommit,
        const std::string& emulatorVersion,
        const ProgressReport& progress,
        const std::vector<BlockerObservation>& observations,
        const ComparisonReport& comparison) const;
};

} // namespace bet
