#pragma once

#include "iboot_detector.h"

class KernelDetector : public IBootDetector {
public:
    KernelDetector() = default;
    ~KernelDetector() override = default;

    std::vector<BootEvent> processLine(std::string_view line, const DetectorContext& context) override;
    int getPriority() const override { return 100; }
};
