#include "boot_pipeline/panic_detector.h"
#include <string>
#include <regex>
#include <cctype>

std::vector<BootEvent> PanicDetector::processLine(std::string_view line, const DetectorContext& context) {
    std::vector<BootEvent> events;

    std::string strLine(line);

    // 1. Detect Kernel Panic start
    bool isPanicLine = false;
    if (strLine.find("Kernel panic - not syncing:") != std::string::npos) {
        m_panicDetected = true;
        isPanicLine = true;
        size_t idx = strLine.find("Kernel panic - not syncing:");
        m_panicReason = strLine.substr(idx + 27); // Length of "Kernel panic - not syncing:"
        
        // Remove trailing carriage returns/newlines
        while (!m_panicReason.empty() && (m_panicReason.back() == '\r' || m_panicReason.back() == '\n')) {
            m_panicReason.pop_back();
        }
    } else if (strLine.find("panic(") != std::string::npos || strLine.find("Failed to execute /init") != std::string::npos) {
        m_panicDetected = true;
        isPanicLine = true;
        m_panicReason = strLine;
    }

    // 2. Parse process name, PID, CPU core info
    // Example: "CPU: 0 PID: 1 Comm: swapper/0 Not tainted"
    if (strLine.find("CPU:") != std::string::npos && strLine.find("PID:") != std::string::npos) {
        std::regex cpuPidRegex("CPU:\\s*(\\d+)\\s*PID:\\s*(\\d+)\\s*Comm:\\s*([^\\s]+)");
        std::smatch match;
        if (std::regex_search(strLine, match, cpuPidRegex)) {
            m_cpu = match[1].str();
            m_pid = match[2].str();
            m_processName = match[3].str();
        }
    }

    // 3. Collect registers
    // Example lines containing registers like "x0 : " or "pc : "
    if (strLine.find("x0 :") != std::string::npos || strLine.find("x2 :") != std::string::npos || 
        strLine.find("pc :") != std::string::npos || strLine.find("sp :") != std::string::npos) {
        if (!m_registers.empty()) {
            m_registers += "\n";
        }
        m_registers += strLine;
    }

    // 4. Collect call trace
    if (strLine.find("Call trace:") != std::string::npos || strLine.find("Call Trace:") != std::string::npos) {
        m_collectingCallTrace = true;
        m_callTrace += strLine;
    } else if (m_collectingCallTrace) {
        // Trace lines start with spaces or bracket frames: E.g. "[<ffff000008085200>]"
        if (strLine.find("[<") != std::string::npos || (strLine.length() > 2 && std::isspace(strLine[0]) && std::isspace(strLine[1]))) {
            m_callTrace += "\n" + strLine;
        } else {
            m_collectingCallTrace = false;
        }
    }

    // 5. Emit Panic Event on trigger
    // Note: We emit a Panic event on the first occurrence of panic OR if it's the main panic line to prevent flooding.
    if (m_panicDetected && (isPanicLine || events.empty())) {
        BootEvent ev;
        ev.type = BootEventType::KernelPanic;
        ev.severity = EventSeverity::Critical;
        ev.confidence = 1.0;
        ev.elapsedSeconds = context.elapsedSeconds;
        ev.details = "Kernel Panic: " + m_panicReason;

        // Suggested cause helper
        std::string suggestedCause = "Check device trees parameters and virtual board configurations.";
        if (m_panicReason.find("Unable to mount root fs") != std::string::npos || m_panicReason.find("VFS: ") != std::string::npos) {
            suggestedCause = "Missing root filesystem. Ensure initrd ramdisk is compiled and passed correctly.";
        } else if (m_panicReason.find("Attempted to kill init!") != std::string::npos || m_panicReason.find("/init") != std::string::npos) {
            suggestedCause = "First-stage init process failed or exited early. Verify init architecture and dependencies.";
        }
        
        ev.metadata["reason"] = m_panicReason;
        ev.metadata["suggested_cause"] = suggestedCause;
        ev.metadata["cpu"] = m_cpu;
        ev.metadata["pid"] = m_pid;
        ev.metadata["process"] = m_processName;
        if (!m_registers.empty()) ev.metadata["registers"] = m_registers;
        if (!m_callTrace.empty()) ev.metadata["call_trace"] = m_callTrace;

        events.push_back(ev);
    }

    return events;
}
