#pragma once

#include <string>
#include <vector>
#include "boot_events/event_types.h"

class BootSession {
public:
    BootSession() = default;
    ~BootSession() = default;

    void addEvent(const BootEvent& event);
    
    // Setters
    void setRomName(const std::string& name) { m_romName = name; }
    void setKernelVersion(const std::string& version) { m_kernelVersion = version; }
    void setDuration(double seconds) { m_durationSeconds = seconds; }
    void setFinalResult(const std::string& result) { m_finalResult = result; }
    void setLastState(const std::string& state) { m_lastState = state; }
    void setPanicReason(const std::string& reason) { m_panicReason = reason; }

    // Getters
    const std::string& getRomName() const { return m_romName; }
    const std::string& getKernelVersion() const { return m_kernelVersion; }
    double getDuration() const { return m_durationSeconds; }
    const std::string& getFinalResult() const { return m_finalResult; }
    const std::string& getLastState() const { return m_lastState; }
    const std::string& getPanicReason() const { return m_panicReason; }
    const std::vector<BootEvent>& getTimeline() const { return m_timeline; }
    
    int getInfoCount() const { return m_infoCount; }
    int getWarningCount() const { return m_warningCount; }
    int getErrorCount() const { return m_errorCount; }
    int getPanicCount() const { return m_panicCount; }

    // Export boot session logs
    void writeLogFile(const std::string& logsDir, const std::string& fullLogText) const;
    void writeJsonSummary(const std::string& logsDir) const;
    
    // Generate summary statistics text block
    std::string generateStats() const;

private:
    std::string m_romName;
    std::string m_kernelVersion = "Unknown";
    double m_durationSeconds = 0.0;
    std::string m_finalResult = "Unknown";
    std::string m_lastState = "VM_STARTING";
    std::string m_panicReason = "";
    std::vector<BootEvent> m_timeline;

    int m_infoCount = 0;
    int m_warningCount = 0;
    int m_errorCount = 0;
    int m_panicCount = 0;
};
