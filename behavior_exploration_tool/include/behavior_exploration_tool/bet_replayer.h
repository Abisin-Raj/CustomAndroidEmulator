#pragma once

#include "bet_types.h"
#include <vector>
#include <string>

namespace bet {

class BetReplayer {
public:
    BetReplayer() = default;
    ~BetReplayer() = default;

    // Load recorded trace from JSON file. Returns false on read error.
    bool loadRunTrace(const std::string& eventsFilePath);

    // Navigation API
    int getEventCount() const;
    int getCurrentIndex() const { return m_currentIndex; }
    
    // Position pointer controls
    bool jumpToEvent(int index, BetEvent& outEvent);
    bool nextEvent(BetEvent& outEvent);
    bool prevEvent(BetEvent& outEvent);

    // Filtering controls
    void setCategoryFilter(const std::string& category);
    void setCpuFilter(int cpu);
    void setPcFilter(uint64_t pc);
    void clearFilters();

    // Query/Search API
    std::vector<BetEvent> searchEvents(const std::string& query) const;

    // Export individual event details
    std::string exportEventJson(int index) const;

private:
    void applyFiltering();
    bool matchesFilters(const BetEvent& ev) const;

    std::vector<BetEvent> m_allEvents;
    std::vector<int>      m_filteredIndices; // indices of events in m_allEvents matching filters
    int                   m_currentIndex = -1;

    // Active filters
    std::string m_filterCategory;
    int         m_filterCpu = -1;
    bool        m_hasPcFilter = false;
    uint64_t    m_filterPc = 0;
};

} // namespace bet
