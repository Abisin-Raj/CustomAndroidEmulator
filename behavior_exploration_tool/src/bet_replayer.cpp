#include "behavior_exploration_tool/bet_replayer.h"
#include "behavior_exploration_tool/event_recorder.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace bet {

using json = nlohmann::json;

bool BetReplayer::loadRunTrace(const std::string& filepath) {
    m_allEvents.clear();
    m_currentIndex = -1;
    clearFilters();

    try {
        std::ifstream in(filepath);
        if (!in.is_open()) {
            spdlog::error("[BET] BetReplayer: Failed to open trace file: {}", filepath);
            return false;
        }

        json jArray;
        in >> jArray;
        in.close();

        if (!jArray.is_array()) {
            spdlog::error("[BET] BetReplayer: Root JSON is not an array");
            return false;
        }

        for (const auto& j : jArray) {
            BetEvent ev;
            EventRecorder::from_json(j, ev);
            m_allEvents.push_back(ev);
        }

        applyFiltering();
        if (!m_filteredIndices.empty()) {
            m_currentIndex = 0;
        }

        spdlog::info("[BET] BetReplayer: Loaded {} events from trace file", m_allEvents.size());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[BET] BetReplayer: Exception loading trace file: {}", e.what());
        return false;
    }
}

int BetReplayer::getEventCount() const {
    return static_cast<int>(m_filteredIndices.size());
}

bool BetReplayer::jumpToEvent(int index, BetEvent& outEvent) {
    if (index < 0 || index >= getEventCount()) {
        return false;
    }
    m_currentIndex = index;
    outEvent = m_allEvents[m_filteredIndices[m_currentIndex]];
    return true;
}

bool BetReplayer::nextEvent(BetEvent& outEvent) {
    return jumpToEvent(m_currentIndex + 1, outEvent);
}

bool BetReplayer::prevEvent(BetEvent& outEvent) {
    return jumpToEvent(m_currentIndex - 1, outEvent);
}

void BetReplayer::setCategoryFilter(const std::string& category) {
    m_filterCategory = category;
    applyFiltering();
}

void BetReplayer::setCpuFilter(int cpu) {
    m_filterCpu = cpu;
    applyFiltering();
}

void BetReplayer::setPcFilter(uint64_t pc) {
    m_filterPc = pc;
    m_hasPcFilter = true;
    applyFiltering();
}

void BetReplayer::clearFilters() {
    m_filterCategory.clear();
    m_filterCpu = -1;
    m_hasPcFilter = false;
    m_filterPc = 0;
    applyFiltering();
}

void BetReplayer::applyFiltering() {
    m_filteredIndices.clear();
    for (size_t i = 0; i < m_allEvents.size(); ++i) {
        if (matchesFilters(m_allEvents[i])) {
            m_filteredIndices.push_back(static_cast<int>(i));
        }
    }
    m_currentIndex = m_filteredIndices.empty() ? -1 : 0;
}

bool BetReplayer::matchesFilters(const BetEvent& ev) const {
    if (!m_filterCategory.empty()) {
        if (ev.category != m_filterCategory) return false;
    }
    if (m_filterCpu >= 0) {
        if (ev.cpu != m_filterCpu) return false;
    }
    if (m_hasPcFilter) {
        if (ev.pc != m_filterPc) return false;
    }
    return true;
}

std::vector<BetEvent> BetReplayer::searchEvents(const std::string& query) const {
    std::vector<BetEvent> matches;
    if (query.empty()) return matches;
    
    std::string qLower = query;
    std::transform(qLower.begin(), qLower.end(), qLower.begin(), ::tolower);

    // Search through currently filtered set
    for (int idx : m_filteredIndices) {
        const auto& ev = m_allEvents[idx];
        
        std::string catLower = ev.category;
        std::transform(catLower.begin(), catLower.end(), catLower.begin(), ::tolower);
        
        std::string nameLower = ev.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        
        std::string detailsLower = ev.details;
        std::transform(detailsLower.begin(), detailsLower.end(), detailsLower.begin(), ::tolower);

        if (catLower.find(qLower) != std::string::npos ||
            nameLower.find(qLower) != std::string::npos ||
            detailsLower.find(qLower) != std::string::npos) {
            matches.push_back(ev);
        }
    }
    return matches;
}

std::string BetReplayer::exportEventJson(int index) const {
    if (index < 0 || index >= getEventCount()) {
        return "{}";
    }
    
    const auto& ev = m_allEvents[m_filteredIndices[index]];
    json jEv;
    EventRecorder::to_json(jEv, ev);
    return jEv.dump(4);
}

} // namespace bet
