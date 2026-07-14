#pragma once

#include <string>
#include <vector>
#include <string_view>
#include "boot_events/event_types.h"

struct DetectorContext {
    double elapsedSeconds = 0.0;
    BootState currentState = BootState::VM_STARTING;
    std::string kernelVersion = "Unknown";
    bool initDetected = false;
    bool panicDetected = false;
};

class IBootDetector {
public:
    virtual ~IBootDetector() = default;
    
    // Process a log line and return any detected events.
    virtual std::vector<BootEvent> processLine(std::string_view line, const DetectorContext& context) = 0;
    
    // High values run first.
    virtual int getPriority() const = 0;
};
