#pragma once
#include "smc_call_record.h"
#include <cstdint>
#include <string>
#include <optional>

namespace emulator::smc {

struct SmcRetryAnomaly {
    uint32_t opcode{0};
    std::string opcode_name;
    uint64_t count{0};
    uint64_t first_pc{0};
    std::string likely_cause;
};

class SmcRetryDetector {
public:
    explicit SmcRetryDetector(uint32_t threshold = 10);

    void SetThreshold(uint32_t threshold) { threshold_ = threshold; }
    uint32_t GetThreshold() const { return threshold_; }

    std::optional<SmcRetryAnomaly> Track(const SmcCallRecord& call);
    void Reset();

private:
    uint32_t threshold_{10};
    uint32_t current_opcode_{0};
    uint64_t current_count_{0};
    uint64_t first_pc_{0};
    bool anomaly_reported_{false};
};

} // namespace emulator::smc
