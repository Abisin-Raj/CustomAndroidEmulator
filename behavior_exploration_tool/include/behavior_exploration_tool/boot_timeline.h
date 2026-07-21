#pragma once

#include "bet_types.h"
#include <vector>
#include <string>

namespace bet {

class BootTimeline {
public:
    BootTimeline() = default;
    ~BootTimeline() = default;

    // Build chronological timeline list fromRecorded events
    void buildTimeline(const std::vector<BetEvent>& events);
    const std::vector<TimelineEvent>& getTimelineEvents() const { return m_timeline; }
    void clear() { m_timeline.clear(); }

    std::string generateTimelineText() const;

private:
    std::vector<TimelineEvent> m_timeline;
};

} // namespace bet
