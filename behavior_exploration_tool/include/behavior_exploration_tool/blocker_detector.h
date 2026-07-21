#pragma once

#include "bet_types.h"
#include <vector>
#include <string>

namespace bet {

struct BlockerObservation {
    std::string type; // "Repeated SMC", "Repeated PC", "Panic", "Watchdog Timeout"
    std::string description;
    std::string evidence;
};

class BlockerDetector {
public:
    BlockerDetector() = default;
    ~BlockerDetector() = default;

    // Analyze events and logs to detect potential blockers / hangs
    void detectBlockers(const std::vector<BetEvent>& events);
    
    const std::vector<BlockerObservation>& getObservations() const { return m_observations; }
    void clear() { m_observations.clear(); }

private:
    std::vector<BlockerObservation> m_observations;
};

} // namespace bet
