#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <iostream>
#include <chrono>
#include <nlohmann/json.hpp>

namespace emulator::milestones {

enum class BootMilestone {
    KERNEL_BOOT,
    FIRST_STAGE_INIT,
    SECOND_STAGE_INIT,
    ZYGOTE,
    SYSTEM_SERVER,
    LAUNCHER3,
    BINDER_TRANSACTION,
    SURFACEFLINGER_COMPOSITE,
    FIRST_FRAME_SCANOUT,
    COUNT
};

struct MilestoneStatus {
    std::string name;
    bool achieved{false};
    std::string proof_detail;
    std::string timestamp;
};

class BootMilestoneTracker {
public:
    static BootMilestoneTracker& Instance();

    void MarkAchieved(BootMilestone milestone, const std::string& proof_detail);
    bool IsAchieved(BootMilestone milestone) const;

    void PrintProgress() const;
    nlohmann::json ToJson() const;

private:
    BootMilestoneTracker();
    ~BootMilestoneTracker() = default;

    mutable std::mutex mutex_;
    std::map<BootMilestone, MilestoneStatus> milestones_;
};

} // namespace emulator::milestones
