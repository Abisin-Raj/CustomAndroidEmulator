#include "boot_milestone_tracker.h"
#include <iomanip>
#include <sstream>

namespace emulator::milestones {

static std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

BootMilestoneTracker& BootMilestoneTracker::Instance() {
    static BootMilestoneTracker instance;
    return instance;
}

BootMilestoneTracker::BootMilestoneTracker()
    : session_start_(std::chrono::steady_clock::now()) {
    ResetSession();
}

void BootMilestoneTracker::ResetSession() {
    std::lock_guard<std::mutex> lock(mutex_);
    session_start_ = std::chrono::steady_clock::now();
    milestones_.clear();

    milestones_[BootMilestone::KERNEL_BOOT] = {"Kernel", false, 0.0, "", ""};
    milestones_[BootMilestone::FIRST_STAGE_INIT] = {"First Stage", false, 0.0, "", ""};
    milestones_[BootMilestone::SECOND_STAGE_INIT] = {"Second Stage", false, 0.0, "", ""};
    milestones_[BootMilestone::ZYGOTE] = {"Zygote", false, 0.0, "", ""};
    milestones_[BootMilestone::SYSTEM_SERVER] = {"system_server", false, 0.0, "", ""};
    milestones_[BootMilestone::LAUNCHER3] = {"Launcher3", false, 0.0, "", ""};
    milestones_[BootMilestone::BINDER_TRANSACTION] = {"Binder Transaction", false, 0.0, "", ""};
    milestones_[BootMilestone::SURFACEFLINGER_COMPOSITE] = {"SurfaceFlinger Composite", false, 0.0, "", ""};
    milestones_[BootMilestone::FIRST_FRAME_SCANOUT] = {"First Frame", false, 0.0, "", ""};
    milestones_[BootMilestone::SYSTEMUI_PROCESS] = {"SystemUI Process", false, 0.0, "", ""};
    milestones_[BootMilestone::STATUS_BAR_SURFACE] = {"Status Bar Layer", false, 0.0, "", ""};
    milestones_[BootMilestone::NAV_BAR_SURFACE] = {"Navigation Bar Layer", false, 0.0, "", ""};
    milestones_[BootMilestone::EVENTHUB_OPEN] = {"EventHub /dev/input", false, 0.0, "", ""};
    milestones_[BootMilestone::INPUT_DISPATCHER_ROUTING] = {"InputDispatcher Active", false, 0.0, "", ""};
    milestones_[BootMilestone::LAUNCHER_TOUCH_INTERACTION] = {"Launcher3 Interaction", false, 0.0, "", ""};
}

void BootMilestoneTracker::MarkAchieved(BootMilestone milestone, const std::string& proof_detail) {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - session_start_).count() / 1000.0;
    MarkAchievedWithTime(milestone, elapsed, proof_detail);
}

void BootMilestoneTracker::MarkAchievedWithTime(BootMilestone milestone, double elapsed_sec, const std::string& proof_detail) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (milestones_.find(milestone) != milestones_.end()) {
        milestones_[milestone].achieved = true;
        milestones_[milestone].elapsed_seconds = elapsed_sec;
        milestones_[milestone].proof_detail = proof_detail;
        milestones_[milestone].timestamp = GetCurrentTimestamp();
    }
}

bool BootMilestoneTracker::IsAchieved(BootMilestone milestone) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = milestones_.find(milestone);
    if (it != milestones_.end()) {
        return it->second.achieved;
    }
    return false;
}

double BootMilestoneTracker::GetElapsedSeconds(BootMilestone milestone) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = milestones_.find(milestone);
    if (it != milestones_.end()) {
        return it->second.elapsed_seconds;
    }
    return 0.0;
}

void BootMilestoneTracker::PrintProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "\n================================================================================\n";
    std::cout << "TIMED BOOT PROGRESS & REGRESSION TRACKER\n";
    std::cout << "================================================================================\n";
    std::cout << "  " << std::left << std::setw(6) << "Mark"
              << std::setw(28) << "Milestone"
              << std::setw(12) << "Time (s)"
              << "Verification Proof\n";
    std::cout << "  ----------------------------------------------------------------------------\n";
    for (const auto& [m, status] : milestones_) {
        std::string mark = status.achieved ? "[✓]" : "[ ]";
        std::cout << "  " << std::left << std::setw(6) << mark
                  << std::setw(28) << status.name;
        if (status.achieved) {
            std::ostringstream time_ss;
            time_ss << std::fixed << std::setprecision(1) << status.elapsed_seconds << " s";
            std::cout << std::setw(12) << time_ss.str();
        } else {
            std::cout << std::setw(12) << "--";
        }
        if (status.achieved && !status.proof_detail.empty()) {
            std::cout << status.proof_detail;
        }
        std::cout << "\n";
    }
    std::cout << "================================================================================\n\n" << std::flush;
}

nlohmann::json BootMilestoneTracker::ToJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j = nlohmann::json::object();
    for (const auto& [m, status] : milestones_) {
        j[status.name] = {
            {"achieved", status.achieved},
            {"elapsed_seconds", status.elapsed_seconds},
            {"proof_detail", status.proof_detail},
            {"timestamp", status.timestamp}
        };
    }
    return j;
}

} // namespace emulator::milestones
