#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace bet {

enum class BetEventType {
    BootStart,
    BootEnd,
    VmStateChange,
    GuestIntercept,
    SerialLogLine
};

inline std::string betEventTypeToString(BetEventType type) {
    switch (type) {
        case BetEventType::BootStart:      return "BootStart";
        case BetEventType::BootEnd:        return "BootEnd";
        case BetEventType::VmStateChange:  return "VmStateChange";
        case BetEventType::GuestIntercept: return "GuestIntercept";
        case BetEventType::SerialLogLine:  return "SerialLogLine";
    }
    return "Unknown";
}

struct BetEvent {
    int bootId = 0;
    double elapsedSeconds = 0.0;
    std::string timestamp; // ISO 8601 string
    BetEventType type = BetEventType::BootStart;
    
    // Generic grouping fields
    std::string category;  // e.g. "SMC", "PSCI", "SERIAL", "STATE"
    std::string name;      // e.g. "0xC2000601", "VM_STARTING", "serial_log"
    
    // CPU/execution state
    int cpu = -1;
    uint64_t pc = 0;
    uint64_t lr = 0;
    uint64_t sp = 0;
    
    // Register snapshots
    std::vector<uint64_t> registersBefore;
    std::vector<uint64_t> registersDispatcher;
    std::vector<uint64_t> registersAfter;
    bool handled = false;
    std::string overrideRuleId;
    
    // Raw information
    std::string details;
    std::map<std::string, std::string> metadata;
};

struct TimelineEvent {
    double elapsedSeconds = 0.0;
    std::string timestamp;
    std::string category;
    std::string description;
};

struct ProgressReport {
    double durationSeconds = 0.0;
    int totalSmcCount = 0;
    int uniqueSmcCount = 0;
    uint64_t lastPc = 0;
    std::string lastSymbol;
    std::string lastSerialLine;
    int initializedDriversCount = 0;
    std::vector<std::string> initializedDrivers;
};

struct ComparisonReport {
    bool hasPreviousRun = false;
    int previousBootId = 0;
    std::string progressStatus; // "Progressed", "Regressed", "No Change"
    int smcCountDelta = 0;
    int driverCountDelta = 0;
    bool reachedNewPc = false;
    std::vector<std::string> newDrivers;
};

} // namespace bet
