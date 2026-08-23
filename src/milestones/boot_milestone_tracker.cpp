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

BootMilestoneTracker::BootMilestoneTracker() {
    milestones_[BootMilestone::KERNEL_BOOT] = {"Kernel", false, "", ""};
    milestones_[BootMilestone::FIRST_STAGE_INIT] = {"First Stage Init", false, "", ""};
    milestones_[BootMilestone::SECOND_STAGE_INIT] = {"Second Stage Init", false, "", ""};
    milestones_[BootMilestone::ZYGOTE] = {"Zygote", false, "", ""};
    milestones_[BootMilestone::SYSTEM_SERVER] = {"system_server", false, "", ""};
    milestones_[BootMilestone::LAUNCHER3] = {"Launcher3", false, "", ""};
    milestones_[BootMilestone::BINDER_TRANSACTION] = {"Binder Transaction", false, "", ""};
    milestones_[BootMilestone::SURFACEFLINGER_COMPOSITE] = {"SurfaceFlinger Composite", false, "", ""};
    milestones_[BootMilestone::FIRST_FRAME_SCANOUT] = {"First Frame", false, "", ""};
}

void BootMilestoneTracker::MarkAchieved(BootMilestone milestone, const std::string& proof_detail) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (milestones_.find(milestone) != milestones_.end()) {
        milestones_[milestone].achieved = true;
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

void BootMilestoneTracker::PrintProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "\n================================================================================\n";
    std::cout << "BOOT PROGRESS TRACKER\n";
    std::cout << "================================================================================\n";
    for (const auto& [m, status] : milestones_) {
        std::string mark = status.achieved ? "[✓]" : "[ ]";
        std::cout << "  " << mark << " " << std::left << std::setw(28) << status.name;
        if (status.achieved && !status.proof_detail.empty()) {
            std::cout << " (" << status.proof_detail << ")";
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
            {"proof_detail", status.proof_detail},
            {"timestamp", status.timestamp}
        };
    }
    return j;
}

} // namespace emulator::milestones
