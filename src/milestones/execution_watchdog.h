#pragma once
#include <cstdint>
#include <string>
#include <chrono>
#include <nlohmann/json.hpp>

namespace emulator::milestones {

enum class WatchdogState {
    ACTIVE_EXECUTION,
    WAITING_EPOLL,
    WAITING_FUTEX,
    WAITING_BINDER,
    STALLED_DEADLOCK,
    SPINLOCK_LOOP
};

struct GuestProgressReport {
    WatchdogState state{WatchdogState::ACTIVE_EXECUTION};
    std::string state_name{"ACTIVE EXECUTION"};
    std::string current_phase{"Kernel Boot"};
    double time_since_milestone_sec{0.0};
    uint64_t last_pc{0};
    std::string last_symbol;
    uint32_t last_syscall_nr{0};
    std::string last_syscall_name{"none"};
    std::string last_binder_target{"none"};
    std::string likely_state{"Bootstrapping kernel"};

    std::string FormatConsole() const;
    nlohmann::json ToJson() const;
};

class ExecutionWatchdog {
public:
    explicit ExecutionWatchdog(uint64_t stall_threshold_ms = 10000);

    void SetCurrentPhase(const std::string& phase);
    void OnMilestoneAdvanced(const std::string& new_phase);

    GuestProgressReport SampleGuestState(
        uint64_t current_pc,
        uint32_t syscall_nr = 0,
        const std::string& binder_target = "",
        const std::string& pc_symbol = ""
    );

    static std::string ResolveSyscallName(uint32_t syscall_nr);

private:
    uint64_t stall_threshold_ms_{10000};
    std::string current_phase_{"Kernel Boot"};
    std::chrono::steady_clock::time_point last_milestone_time_;
    std::chrono::steady_clock::time_point last_change_time_;

    uint64_t last_pc_{0};
    uint32_t last_syscall_nr_{0};
    std::string last_binder_target_{"none"};
    uint64_t identical_samples_{0};
};

} // namespace emulator::milestones
