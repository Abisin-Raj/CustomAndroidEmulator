#include "boot_pipeline/kernel_detector.h"
#include <string>

std::vector<BootEvent> KernelDetector::processLine(std::string_view line, const DetectorContext& context) {
    std::vector<BootEvent> events;

    // 1. Linux version check
    if (line.find("Linux version") != std::string_view::npos) {
        BootEvent ev;
        ev.type = BootEventType::KernelVersion;
        ev.severity = EventSeverity::Info;
        ev.confidence = 0.99;
        ev.elapsedSeconds = context.elapsedSeconds;
        
        // Extract kernel version substring
        size_t start = line.find("Linux version");
        size_t end = line.find(" (", start);
        if (end != std::string_view::npos) {
            ev.details = std::string(line.substr(start, end - start));
        } else {
            ev.details = std::string(line.substr(start));
        }
        events.push_back(ev);
    }

    // 2. CPU Online check
    if (line.find("Booting Linux on physical CPU") != std::string_view::npos ||
        line.find("CPU0: Booted secondary processor") != std::string_view::npos ||
        line.find("CPU0 online") != std::string_view::npos) {
        BootEvent ev;
        ev.type = BootEventType::CpuOnline;
        ev.severity = EventSeverity::Info;
        ev.confidence = 0.95;
        ev.elapsedSeconds = context.elapsedSeconds;
        ev.details = "CPU Core Booted and Online";
        events.push_back(ev);
    }

    // 3. PSCI Initialized check
    if (line.find("psci: PSCI") != std::string_view::npos ||
        line.find("PSCI initialized") != std::string_view::npos) {
        BootEvent ev;
        ev.type = BootEventType::PsciInitialized;
        ev.severity = EventSeverity::Info;
        ev.confidence = 0.98;
        ev.elapsedSeconds = context.elapsedSeconds;
        ev.details = "PSCI Virtualization Controller Initialized";
        events.push_back(ev);
    }

    return events;
}
