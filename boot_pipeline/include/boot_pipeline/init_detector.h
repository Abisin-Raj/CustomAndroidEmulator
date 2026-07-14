#pragma once

#include "iboot_detector.h"

class InitDetector : public IBootDetector {
public:
    InitDetector() = default;
    ~InitDetector() override = default;

    std::vector<BootEvent> processLine(std::string_view line, const DetectorContext& context) override;
    int getPriority() const override { return 50; }
};
