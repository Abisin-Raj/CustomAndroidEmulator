#pragma once

#include "bet_types.h"
#include <vector>
#include <string>

namespace bet {

class ProgressAnalyzer {
public:
    ProgressAnalyzer() = default;
    ~ProgressAnalyzer() = default;

    // Analyze boot events and serial logs to measure progress
    void analyzeProgress(const std::vector<BetEvent>& events);
    
    const ProgressReport& getReport() const { return m_report; }
    void clear();

private:
    ProgressReport m_report;
};

} // namespace bet
