#pragma once
#include "scsi_command_record.h"
#include <vector>
#include <deque>
#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>

namespace emulator::storage {

class UfsTelemetry {
public:
    static UfsTelemetry& Instance();

    void SetHistoryDepth(size_t depth);
    size_t GetHistoryDepth() const { return max_history_depth_; }

    void LogDoorbellWrite(uint32_t slot_mask);
    void LogSlotAllocated(uint8_t slot, uint8_t lun, uint8_t scsi_op);
    void LogScsiCommand(ScsiCommandRecord cmd);
    void LogDmaTransfer(uint64_t dma_addr, size_t byte_count, bool is_write);
    void LogCompletionInterrupt(uint32_t status_mask);

    std::vector<ScsiCommandRecord> GetHistory() const;
    void Clear();

    nlohmann::json ToJson() const;
    void SaveHistoryJson(const std::filesystem::path& file_path) const;

private:
    UfsTelemetry();
    ~UfsTelemetry() = default;

    UfsTelemetry(const UfsTelemetry&) = delete;
    UfsTelemetry& operator=(const UfsTelemetry&) = delete;

    mutable std::mutex mutex_;
    size_t max_history_depth_{32};
    std::deque<ScsiCommandRecord> history_;
};

} // namespace emulator::storage
