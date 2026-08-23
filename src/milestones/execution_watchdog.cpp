#include "execution_watchdog.h"
#include <iomanip>
#include <sstream>

namespace emulator::milestones {

static std::string FormatHex(uint64_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << val;
    return ss.str();
}

ExecutionWatchdog::ExecutionWatchdog(uint64_t stall_threshold_ms)
    : stall_threshold_ms_(stall_threshold_ms),
      last_change_time_(std::chrono::steady_clock::now()) {}

void ExecutionWatchdog::Reset() {
    last_pc_ = 0;
    last_syscalls_ = 0;
    last_binder_tx_ = 0;
    identical_pc_count_ = 0;
    last_change_time_ = std::chrono::steady_clock::now();
}

WatchdogReport ExecutionWatchdog::Check(uint64_t current_pc, uint64_t total_syscalls, uint64_t total_binder_tx) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_change_time_).count();

    bool pc_changed = (current_pc != last_pc_ && current_pc != 0);
    bool syscalls_changed = (total_syscalls != last_syscalls_);
    bool binder_changed = (total_binder_tx != last_binder_tx_);

    if (pc_changed || syscalls_changed || binder_changed) {
        last_pc_ = current_pc;
        last_syscalls_ = total_syscalls;
        last_binder_tx_ = total_binder_tx;
        last_change_time_ = now;
        identical_pc_count_ = 0;

        WatchdogReport r;
        r.state = WatchdogState::ACTIVE_EXECUTION;
        r.state_name = "ACTIVE EXECUTION";
        r.last_pc = current_pc;
        r.stall_duration_ms = 0;
        r.detail = "Guest CPU actively executing instructions (PC=" + FormatHex(current_pc) + ")";
        return r;
    }

    identical_pc_count_++;

    WatchdogReport r;
    r.last_pc = last_pc_;
    r.stall_duration_ms = elapsed_ms;

    if (elapsed_ms >= static_cast<int64_t>(stall_threshold_ms_)) {
        r.state = WatchdogState::DEADLOCK;
        r.state_name = "DEADLOCK";
        std::ostringstream ss;
        ss << "PC unchanged at " << FormatHex(last_pc_) << " for " << elapsed_ms
           << " ms with zero new syscalls or Binder transactions";
        r.detail = ss.str();
    } else if (identical_pc_count_ >= 50) {
        r.state = WatchdogState::SPINLOCK_LOOP;
        r.state_name = "SPINLOCK LOOP";
        r.detail = "Instruction pointer spinning repeatedly at " + FormatHex(last_pc_);
    } else {
        r.state = WatchdogState::ACTIVE_EXECUTION;
        r.state_name = "ACTIVE EXECUTION";
        r.detail = "Executing within expected latency window";
    }

    return r;
}

} // namespace emulator::milestones
