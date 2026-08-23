#include "execution_watchdog.h"
#include <iomanip>
#include <sstream>

namespace emulator::milestones {

static std::string FormatHex(uint64_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << val;
    return ss.str();
}

std::string ExecutionWatchdog::ResolveSyscallName(uint32_t syscall_nr) {
    switch (syscall_nr) {
        case 20:  return "epoll_create1";
        case 21:  return "epoll_ctl";
        case 22:  return "epoll_pwait";
        case 24:  return "dup";
        case 29:  return "ioctl (binder/mmio)";
        case 40:  return "mount";
        case 48:  return "faccessat";
        case 56:  return "openat";
        case 57:  return "close";
        case 59:  return "pipe2";
        case 63:  return "read";
        case 64:  return "write";
        case 65:  return "readv";
        case 66:  return "writev";
        case 78:  return "readlinkat";
        case 79:  return "newfstatat";
        case 93:  return "exit_group";
        case 94:  return "exit";
        case 98:  return "futex";
        case 101: return "nanosleep";
        case 113: return "clock_gettime";
        case 134: return "rt_sigaction";
        case 135: return "rt_sigprocmask";
        case 160: return "uname";
        case 214: return "brk";
        case 215: return "munmap";
        case 220: return "clone";
        case 221: return "execve";
        case 222: return "mmap";
        case 226: return "mprotect";
        case 241: return "epoll_pwait64";
        case 260: return "wait4";
        case 278: return "getrandom";
        default: {
            if (syscall_nr == 0) return "none";
            return "syscall_" + std::to_string(syscall_nr);
        }
    }
}

ExecutionWatchdog::ExecutionWatchdog(uint64_t stall_threshold_ms)
    : stall_threshold_ms_(stall_threshold_ms),
      last_milestone_time_(std::chrono::steady_clock::now()),
      last_change_time_(std::chrono::steady_clock::now()) {}

void ExecutionWatchdog::SetCurrentPhase(const std::string& phase) {
    current_phase_ = phase;
    last_milestone_time_ = std::chrono::steady_clock::now();
}

void ExecutionWatchdog::OnMilestoneAdvanced(const std::string& new_phase) {
    current_phase_ = new_phase;
    last_milestone_time_ = std::chrono::steady_clock::now();
    last_change_time_ = std::chrono::steady_clock::now();
    identical_samples_ = 0;
}

GuestProgressReport ExecutionWatchdog::SampleGuestState(
    uint64_t current_pc,
    uint32_t syscall_nr,
    const std::string& binder_target,
    const std::string& pc_symbol
) {
    auto now = std::chrono::steady_clock::now();
    double milestone_sec = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_milestone_time_).count() / 1000.0;
    auto stall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_change_time_).count();

    bool pc_changed = (current_pc != last_pc_ && current_pc != 0);
    bool syscall_changed = (syscall_nr != last_syscall_nr_ && syscall_nr != 0);
    bool binder_changed = (!binder_target.empty() && binder_target != last_binder_target_);

    if (pc_changed || syscall_changed || binder_changed) {
        last_pc_ = current_pc;
        if (syscall_nr != 0) last_syscall_nr_ = syscall_nr;
        if (!binder_target.empty()) last_binder_target_ = binder_target;
        last_change_time_ = now;
        identical_samples_ = 0;
    } else {
        identical_samples_++;
    }

    GuestProgressReport r;
    r.current_phase = current_phase_;
    r.time_since_milestone_sec = milestone_sec;
    r.last_pc = last_pc_;
    r.last_symbol = pc_symbol;
    r.last_syscall_nr = last_syscall_nr_;
    r.last_syscall_name = ResolveSyscallName(last_syscall_nr_);
    r.last_binder_target = last_binder_target_;

    if (stall_ms >= static_cast<int64_t>(stall_threshold_ms_)) {
        r.state = WatchdogState::STALLED_DEADLOCK;
        r.state_name = "STALLED DEADLOCK";
        r.likely_state = "Execution stalled: PC unchanged at " + FormatHex(last_pc_) + " for " + std::to_string(stall_ms / 1000) + "s";
    } else if (last_syscall_nr_ == 22 || last_syscall_nr_ == 241) {
        r.state = WatchdogState::WAITING_EPOLL;
        r.state_name = "WAITING EPOLL";
        r.likely_state = "Waiting for incoming event/Looper message on epoll_wait";
    } else if (last_syscall_nr_ == 98) {
        r.state = WatchdogState::WAITING_FUTEX;
        r.state_name = "WAITING FUTEX";
        r.likely_state = "Waiting on userspace thread lock (futex synchronization)";
    } else if (last_syscall_nr_ == 29) {
        r.state = WatchdogState::WAITING_BINDER;
        r.state_name = "WAITING BINDER";
        r.likely_state = "Executing / waiting on Binder IPC transaction";
    } else if (identical_samples_ > 30) {
        r.state = WatchdogState::SPINLOCK_LOOP;
        r.state_name = "SPINLOCK LOOP";
        r.likely_state = "Tightly spinning instruction loop at " + FormatHex(last_pc_);
    } else {
        r.state = WatchdogState::ACTIVE_EXECUTION;
        r.state_name = "ACTIVE EXECUTION";
        r.likely_state = "Active ART bytecode execution / service startup";
    }

    return r;
}

std::string GuestProgressReport::FormatConsole() const {
    std::ostringstream ss;
    ss << "\n================================================================================\n";
    ss << "GUEST PROGRESS & WATCHDOG REPORT\n";
    ss << "================================================================================\n";
    ss << "  Current Phase:          " << current_phase << "\n";
    ss << "  Time Since Milestone:   " << std::fixed << std::setprecision(1) << time_since_milestone_sec << " s\n";
    ss << "  Last Program Counter:   " << FormatHex(last_pc);
    if (!last_symbol.empty()) ss << " (" << last_symbol << ")";
    ss << "\n";
    ss << "  Last Syscall:           " << last_syscall_name << " (" << last_syscall_nr << ")\n";
    if (last_binder_target != "none") {
        ss << "  Last Binder Target:     " << last_binder_target << "\n";
    }
    ss << "  Guest Execution State:  " << likely_state << "\n";
    ss << "================================================================================\n";
    return ss.str();
}

nlohmann::json GuestProgressReport::ToJson() const {
    return nlohmann::json{
        {"current_phase", current_phase},
        {"time_since_milestone_sec", time_since_milestone_sec},
        {"last_pc", FormatHex(last_pc)},
        {"last_symbol", last_symbol},
        {"last_syscall_nr", last_syscall_nr},
        {"last_syscall_name", last_syscall_name},
        {"last_binder_target", last_binder_target},
        {"state_name", state_name},
        {"likely_state", likely_state}
    };
}

} // namespace emulator::milestones
