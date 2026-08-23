#include "smc_retry_detector.h"
#include <sstream>

namespace emulator::smc {

SmcRetryDetector::SmcRetryDetector(uint32_t threshold)
    : threshold_(threshold) {}

void SmcRetryDetector::Reset() {
    current_opcode_ = 0;
    current_count_ = 0;
    first_pc_ = 0;
    anomaly_reported_ = false;
}

std::optional<SmcRetryAnomaly> SmcRetryDetector::Track(const SmcCallRecord& call) {
    if (call.opcode == current_opcode_) {
        current_count_++;
    } else {
        current_opcode_ = call.opcode;
        current_count_ = 1;
        first_pc_ = call.pc;
        anomaly_reported_ = false;
    }

    if (current_count_ >= threshold_ && !anomaly_reported_) {
        anomaly_reported_ = true;
        SmcRetryAnomaly anomaly;
        anomaly.opcode = current_opcode_;
        anomaly.opcode_name = call.service_name + "::" + call.function_name;
        anomaly.count = current_count_;
        anomaly.first_pc = first_pc_;
        
        std::ostringstream ss;
        ss << call.service_name << " returned unexpected status or availability state";
        anomaly.likely_cause = ss.str();
        return anomaly;
    }

    return std::nullopt;
}

} // namespace emulator::smc
