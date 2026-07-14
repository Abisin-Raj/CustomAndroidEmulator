#include "boot_pipeline/detector_registry.h"
#include <algorithm>

void DetectorRegistry::registerDetector(std::unique_ptr<IBootDetector> detector) {
    m_detectors.push_back(std::move(detector));
    std::sort(m_detectors.begin(), m_detectors.end(), [](const auto& a, const auto& b) {
        return a->getPriority() > b->getPriority();
    });
}

const std::vector<std::unique_ptr<IBootDetector>>& DetectorRegistry::getDetectors() const {
    return m_detectors;
}
