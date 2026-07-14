#pragma once

#include <vector>
#include <memory>
#include "iboot_detector.h"

class DetectorRegistry {
public:
    DetectorRegistry() = default;
    ~DetectorRegistry() = default;

    void registerDetector(std::unique_ptr<IBootDetector> detector);
    const std::vector<std::unique_ptr<IBootDetector>>& getDetectors() const;

private:
    std::vector<std::unique_ptr<IBootDetector>> m_detectors;
};
