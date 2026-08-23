#pragma once
#include <cstdint>
#include <string>
#include <chrono>

namespace emulator::milestones {

enum class WatchdogState {
    ACTIVE_EXECUTION,
    DEADLOCK,
    SPINLOCK_LOOP,
    UNKNOWN
};

struct WatchdogReport {
    WatchdogState state{WatchdogState::ACTIVE_EXECUTION};
    std::string state_name{"ACTIVE EXECUTION"};
    uint64_t last_pc{0};
    uint64_t stall_duration_ms{0};
    std::string detail;
};

class ExecutionWatchdog {
public:
    explicit ExecutionWatchdog(uint64_t stall_threshold_ms = 5000);

    WatchdogReport Check(uint64_t current_pc, uint64_t total_syscalls, uint64_t total_binder_tx);
    void Reset();

private:
    uint64_t stall_threshold_ms_{5000};
    uint64_t last_pc_{0};
    uint64_t last_syscalls_{0};
    uint64_t last_binder_tx_{0};
    std::chrono::steady_clock::time_point last_change_time_;
    uint64_t identical_pc_count_{0};
};

} // namespace emulator::milestones
