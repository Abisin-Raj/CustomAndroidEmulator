#pragma once

#include "bet_types.h"
#include <string>

namespace bet {

class ComparisonEngine {
public:
    ComparisonEngine() = default;
    ~ComparisonEngine() = default;

    // Compare active run report to a historical run report
    ComparisonReport compareRuns(const ProgressReport& current, const ProgressReport& previous);
    
    std::string generateComparisonSummary(const ComparisonReport& report) const;
};

} // namespace bet
