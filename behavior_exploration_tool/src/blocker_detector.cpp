#include "behavior_exploration_tool/blocker_detector.h"
#include <map>
#include <sstream>
#include <spdlog/spdlog.h>

namespace bet {

void BlockerDetector::detectBlockers(const std::vector<BetEvent>& events) {
    m_observations.clear();
    
    std::map<uint64_t, int> pcCounts;
    std::map<std::pair<uint64_t, std::string>, int> smcCallCounts; // (PC, SMC_name) -> count
    
    bool panicDetected = false;
    std::string panicDetails;
    
    double lastEventTime = 0.0;
    
    for (const auto& ev : events) {
        if (ev.elapsedSeconds > lastEventTime) {
            lastEventTime = ev.elapsedSeconds;
        }

        if (ev.type == BetEventType::GuestIntercept) {
            pcCounts[ev.pc]++;
            if (ev.category == "SMC") {
                smcCallCounts[{ev.pc, ev.name}]++;
            }
        } else if (ev.type == BetEventType::SerialLogLine) {
            if (ev.details.find("Kernel panic") != std::string::npos ||
                ev.details.find("panic") != std::string::npos) {
                panicDetected = true;
                panicDetails = ev.details;
            }
        }
    }
    
    // 1. Detect Repeated SMC calls at the same PC (indicates retry loops / probe defers)
    for (const auto& pair : smcCallCounts) {
        uint64_t pc = pair.first.first;
        std::string smcId = pair.first.second;
        int count = pair.second;
        
        if (count >= 5) {
            BlockerObservation obs;
            obs.type = "Repeated SMC";
            
            std::stringstream ssDesc, ssEv;
            ssDesc << "SMC ID " << smcId << " was repeatedly invoked " << std::dec << count << " times at PC 0x" << std::hex << pc;
            obs.description = ssDesc.str();
            
            ssEv << "Guest execution is stuck in a polling / retry loop query on SCM service.";
            obs.evidence = ssEv.str();
            
            m_observations.push_back(obs);
        }
    }
    
    // 2. Detect Repeated PCs (stuck in a spinlock / busy wait loop)
    for (const auto& pair : pcCounts) {
        uint64_t pc = pair.first;
        int count = pair.second;
        
        // If we hit the exact same PC in intercepts many times without progress
        if (count >= 10) {
            BlockerObservation obs;
            obs.type = "Repeated PC";
            
            std::stringstream ssDesc;
            ssDesc << "Program Counter (PC) 0x" << std::hex << pc << " hit " << std::dec << count << " times in guest intercepts";
            obs.description = ssDesc.str();
            obs.evidence = "Execution pattern suggests a CPU core is stuck in a spinlock or tight polling loop.";
            
            m_observations.push_back(obs);
        }
    }
    
    // 3. Detect Panic
    if (panicDetected) {
        BlockerObservation obs;
        obs.type = "Panic";
        obs.description = "Kernel panic was reported in console logs";
        obs.evidence = panicDetails;
        m_observations.push_back(obs);
    }
    
    // 4. Watchdog Timeout / Freeze
    // If the boot session was terminated early or stopped producing logs
    // We can observe this if duration matches timeout but no progress completed
    if (events.size() > 1) {
        double firstTime = events.front().elapsedSeconds;
        double totalSpan = lastEventTime - firstTime;
        if (totalSpan > 15.0 && !panicDetected && m_observations.empty()) {
            BlockerObservation obs;
            obs.type = "Watchdog Timeout";
            obs.description = "Virtual machine execution halted without completing boot sequence";
            obs.evidence = "No further console lines or SMC intercepts were recorded in the final " + std::to_string(totalSpan) + " seconds.";
            m_observations.push_back(obs);
        }
    }
}

} // namespace bet
