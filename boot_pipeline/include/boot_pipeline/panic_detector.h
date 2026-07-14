#pragma once

#include "iboot_detector.h"

class PanicDetector : public IBootDetector {
public:
    PanicDetector() = default;
    ~PanicDetector() override = default;

    std::vector<BootEvent> processLine(std::string_view line, const DetectorContext& context) override;
    int getPriority() const override { return 150; }

private:
    bool m_panicDetected = false;
    std::string m_panicReason = "Unknown Kernel Panic";
    std::string m_processName = "Unknown";
    std::string m_pid = "Unknown";
    std::string m_cpu = "Unknown";
    std::string m_registers;
    std::string m_callTrace;

    bool m_collectingCallTrace = false;
};
