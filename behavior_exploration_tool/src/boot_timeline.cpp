#include "behavior_exploration_tool/boot_timeline.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace bet {

void BootTimeline::buildTimeline(const std::vector<BetEvent>& events) {
    m_timeline.clear();
    
    // Sort events by timestamp/elapsedSeconds to guarantee chronological order
    std::vector<BetEvent> sortedEvents = events;
    std::stable_sort(sortedEvents.begin(), sortedEvents.end(), [](const BetEvent& a, const BetEvent& b) {
        return a.elapsedSeconds < b.elapsedSeconds;
    });

    for (const auto& ev : sortedEvents) {
        TimelineEvent te;
        te.elapsedSeconds = ev.elapsedSeconds;
        te.timestamp = ev.timestamp;
        
        switch (ev.type) {
            case BetEventType::BootStart:
                te.category = "System";
                te.description = "Boot Started";
                m_timeline.push_back(te);
                break;
            case BetEventType::BootEnd:
                te.category = "System";
                te.description = "Boot Stopped: " + ev.details;
                m_timeline.push_back(te);
                break;
            case BetEventType::VmStateChange:
                te.category = "VM State";
                te.description = "State changed to: " + ev.name;
                m_timeline.push_back(te);
                break;
            case BetEventType::GuestIntercept:
                te.category = ev.category;
                if (ev.category == "SMC") {
                    std::stringstream ss;
                    ss << "SMC Intercepted: " << ev.name;
                    if (ev.metadata.count("service_name")) {
                        ss << " (" << ev.metadata.at("service_name") << ")";
                    }
                    if (ev.handled) {
                        ss << " -> Handled";
                    } else {
                        ss << " -> Unhandled";
                    }
                    te.description = ss.str();
                } else {
                    te.description = "Intercepted " + ev.category + ": " + ev.name;
                }
                m_timeline.push_back(te);
                break;
            case BetEventType::SerialLogLine:
                // Extract important kernel milestones from the serial log line to avoid polluting the timeline with raw console text
                if (ev.details.find("Linux version") != std::string::npos ||
                    ev.details.find("Booting Linux") != std::string::npos) {
                    te.category = "Kernel";
                    te.description = "Kernel Entry (Linux booting)";
                    m_timeline.push_back(te);
                } else if (ev.details.find("CPU: All CPU(s) started") != std::string::npos || 
                           ev.details.find("Brought up") != std::string::npos) {
                    te.category = "Kernel";
                    te.description = "All Guest CPUs Online";
                    m_timeline.push_back(te);
                } else if (ev.details.find("Kernel panic") != std::string::npos ||
                           ev.details.find("panic") != std::string::npos) {
                    te.category = "Panic";
                    te.description = "Kernel Panic: " + ev.details;
                    m_timeline.push_back(te);
                } else if (ev.details.find("init:") != std::string::npos && 
                           ev.details.find("entered") != std::string::npos) {
                    te.category = "Userspace";
                    te.description = "Init Process Entered: " + ev.details;
                    m_timeline.push_back(te);
                } else if (ev.details.find("UFS") != std::string::npos && 
                           ev.details.find("probe succeeded") != std::string::npos) {
                    te.category = "Driver";
                    te.description = "UFS Storage Driver Initialized";
                    m_timeline.push_back(te);
                } else if (ev.details.find("probe of") != std::string::npos && 
                           ev.details.find("succeeded") != std::string::npos) {
                    te.category = "Driver";
                    // Extract driver name
                    size_t pos = ev.details.find("probe of");
                    std::string detail = ev.details.substr(pos);
                    te.description = "Driver Probe Success: " + detail;
                    m_timeline.push_back(te);
                }
                break;
        }
    }
}

std::string BootTimeline::generateTimelineText() const {
    std::stringstream ss;
    for (const auto& te : m_timeline) {
        ss << std::fixed << std::setprecision(3) << "[" << te.elapsedSeconds << "s] ";
        ss << std::left << std::setw(12) << ("[" + te.category + "]") << " " << te.description << "\n";
    }
    return ss.str();
}

} // namespace bet
