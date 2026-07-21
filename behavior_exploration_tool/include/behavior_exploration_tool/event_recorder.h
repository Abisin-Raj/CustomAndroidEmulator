#pragma once

#include "bet_types.h"
#include <vector>
#include <string>

#include <nlohmann/json.hpp>

namespace bet {

class EventRecorder {
public:
    EventRecorder() = default;
    ~EventRecorder() = default;

    void recordEvent(const BetEvent& event);
    const std::vector<BetEvent>& getEvents() const { return m_events; }
    void clear() { m_events.clear(); }

    // Save current run events to a JSON file
    bool saveToJsonFile(const std::string& filepath) const;
    
    // Static helper to convert event to JSON representation
    static void to_json(nlohmann::json& j, const BetEvent& e);
    static void from_json(const nlohmann::json& j, BetEvent& e);

private:
    std::vector<BetEvent> m_events;
};

} // namespace bet
