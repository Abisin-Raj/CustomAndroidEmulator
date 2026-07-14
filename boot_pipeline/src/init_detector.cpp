#include "boot_pipeline/init_detector.h"
#include <string>

std::vector<BootEvent> InitDetector::processLine(std::string_view line, const DetectorContext& context) {
    std::vector<BootEvent> events;

    // 1. Ramdisk mount check
    if (line.find("Trying to unpack rootfs image as initramfs") != std::string_view::npos ||
        line.find("Freeing initrd memory") != std::string_view::npos ||
        line.find("Mounted root") != std::string_view::npos) {
        BootEvent ev;
        ev.type = BootEventType::RamdiskMounted;
        ev.severity = EventSeverity::Info;
        ev.confidence = 0.90;
        ev.elapsedSeconds = context.elapsedSeconds;
        ev.details = "Ramdisk filesystem mounted successfully";
        events.push_back(ev);
    }

    // 2. Init Started check
    if (line.find("init: init first stage started!") != std::string_view::npos ||
        line.find("init first stage started") != std::string_view::npos ||
        line.find("init: Entering second stage") != std::string_view::npos) {
        BootEvent ev;
        ev.type = BootEventType::InitStarted;
        ev.severity = EventSeverity::Info;
        ev.confidence = 0.98;
        ev.elapsedSeconds = context.elapsedSeconds;
        ev.details = "AOSP Init process started execution";
        events.push_back(ev);
    }

    // 3. Service Started check
    if (line.find("init: Starting service '") != std::string_view::npos) {
        BootEvent ev;
        ev.type = BootEventType::ServiceStarted;
        ev.severity = EventSeverity::Info;
        ev.confidence = 0.95;
        ev.elapsedSeconds = context.elapsedSeconds;

        // Extract service name
        size_t start = line.find("init: Starting service '");
        if (start != std::string_view::npos) {
            start += 24; // Length of "init: Starting service '"
            size_t end = line.find("'", start);
            if (end != std::string_view::npos) {
                ev.details = "System service started: " + std::string(line.substr(start, end - start));
            } else {
                ev.details = "System service initialized";
            }
        }
        events.push_back(ev);
    }

    // 4. Boot Completed check
    if (line.find("sys.boot_completed=1") != std::string_view::npos ||
        line.find("boot_completed") != std::string_view::npos) {
        BootEvent ev;
        ev.type = BootEventType::BootCompleted;
        ev.severity = EventSeverity::Info;
        ev.confidence = 0.99;
        ev.elapsedSeconds = context.elapsedSeconds;
        ev.details = "Android userspace boot sequence completed";
        events.push_back(ev);
    }

    return events;
}
