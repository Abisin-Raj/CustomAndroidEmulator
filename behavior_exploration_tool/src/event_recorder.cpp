#include "behavior_exploration_tool/event_recorder.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

namespace bet {

using json = nlohmann::json;

void EventRecorder::recordEvent(const BetEvent& event) {
    m_events.push_back(event);
}

void EventRecorder::to_json(json& j, const BetEvent& e) {
    j = json{
        {"bootId", e.bootId},
        {"elapsedSeconds", e.elapsedSeconds},
        {"timestamp", e.timestamp},
        {"type", betEventTypeToString(e.type)},
        {"category", e.category},
        {"name", e.name},
        {"cpu", e.cpu},
        {"pc", fmt::format("0x{:016x}", e.pc)},
        {"lr", fmt::format("0x{:016x}", e.lr)},
        {"sp", fmt::format("0x{:016x}", e.sp)},
        {"registersBefore", e.registersBefore},
        {"registersDispatcher", e.registersDispatcher},
        {"registersAfter", e.registersAfter},
        {"handled", e.handled},
        {"overrideRuleId", e.overrideRuleId},
        {"details", e.details},
        {"metadata", e.metadata}
    };
}

void EventRecorder::from_json(const json& j, BetEvent& e) {
    e.bootId = j.value("bootId", 0);
    e.elapsedSeconds = j.value("elapsedSeconds", 0.0);
    e.timestamp = j.value("timestamp", "");
    
    std::string typeStr = j.value("type", "BootStart");
    if (typeStr == "BootStart")      e.type = BetEventType::BootStart;
    else if (typeStr == "BootEnd")        e.type = BetEventType::BootEnd;
    else if (typeStr == "VmStateChange")  e.type = BetEventType::VmStateChange;
    else if (typeStr == "GuestIntercept") e.type = BetEventType::GuestIntercept;
    else if (typeStr == "SerialLogLine")  e.type = BetEventType::SerialLogLine;
    
    e.category = j.value("category", "");
    e.name = j.value("name", "");
    e.cpu = j.value("cpu", -1);
    
    std::string pcStr = j.value("pc", "0x0");
    std::string lrStr = j.value("lr", "0x0");
    std::string spStr = j.value("sp", "0x0");
    
    try {
        e.pc = std::stoull(pcStr, nullptr, 16);
        e.lr = std::stoull(lrStr, nullptr, 16);
        e.sp = std::stoull(spStr, nullptr, 16);
    } catch (...) {
        e.pc = 0;
        e.lr = 0;
        e.sp = 0;
    }
    
    e.registersBefore = j.value("registersBefore", std::vector<uint64_t>{});
    e.registersDispatcher = j.value("registersDispatcher", std::vector<uint64_t>{});
    e.registersAfter = j.value("registersAfter", std::vector<uint64_t>{});
    e.handled = j.value("handled", false);
    e.overrideRuleId = j.value("overrideRuleId", "");
    e.details = j.value("details", "");
    e.metadata = j.value("metadata", std::map<std::string, std::string>{});
}

bool EventRecorder::saveToJsonFile(const std::string& filepath) const {
    try {
        json jArray = json::array();
        for (const auto& ev : m_events) {
            json jEv;
            to_json(jEv, ev);
            jArray.push_back(jEv);
        }
        
        std::ofstream out(filepath);
        if (!out.is_open()) {
            spdlog::error("[BET] EventRecorder: Failed to open trace file for writing: {}", filepath);
            return false;
        }
        
        out << jArray.dump(4);
        out.close();
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[BET] EventRecorder: Exception saving events to JSON: {}", e.what());
        return false;
    }
}

} // namespace bet
