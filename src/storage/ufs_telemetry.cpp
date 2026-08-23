#include "ufs_telemetry.h"
#include "src/logger/api/unified_logger.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace emulator::storage {

static std::string FormatHex64(uint64_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << val;
    return ss.str();
}

static std::string FormatHex32(uint32_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << val;
    return ss.str();
}

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

UfsTelemetry& UfsTelemetry::Instance() {
    static UfsTelemetry instance;
    return instance;
}

UfsTelemetry::UfsTelemetry()
    : max_history_depth_(32) {}

void UfsTelemetry::SetHistoryDepth(size_t depth) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_history_depth_ = depth;
    while (history_.size() > max_history_depth_) {
        history_.pop_front();
    }
}

void UfsTelemetry::LogDoorbellWrite(uint32_t slot_mask) {
    std::ostringstream ss;
    ss << "UFS Doorbell write: UTP_TRANSFER_REQ_DOORBELL = " << FormatHex32(slot_mask);
    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::STORAGE,
        ::emulator::logger::models::LogLevel::INFO,
        ss.str());
}

void UfsTelemetry::LogSlotAllocated(uint8_t slot, uint8_t lun, uint8_t scsi_op) {
    std::ostringstream ss;
    ss << "Command slot #" << static_cast<int>(slot)
       << " allocated for LUN " << static_cast<int>(lun)
       << " (" << ScsiOpcodeToString(scsi_op) << ")";
    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::STORAGE,
        ::emulator::logger::models::LogLevel::INFO,
        ss.str());
}

void UfsTelemetry::LogScsiCommand(ScsiCommandRecord cmd) {
    if (cmd.timestamp.empty()) {
        cmd.timestamp = GetCurrentTimestamp();
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (history_.size() >= max_history_depth_) {
            history_.pop_front();
        }
        history_.push_back(cmd);
    }

    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::STORAGE,
        ::emulator::logger::models::LogLevel::INFO,
        cmd.FormatLog(), std::nullopt, cmd.latency_ms);
}

void UfsTelemetry::LogDmaTransfer(uint64_t dma_addr, size_t byte_count, bool is_write) {
    std::ostringstream ss;
    ss << "UFS DMA " << (is_write ? "WRITE" : "READ")
       << " | Addr:" << FormatHex64(dma_addr)
       << " | Size:" << byte_count << " bytes (" << (byte_count / 1024) << " KB)";
    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::STORAGE,
        ::emulator::logger::models::LogLevel::INFO,
        ss.str());
}

void UfsTelemetry::LogCompletionInterrupt(uint32_t status_mask) {
    std::ostringstream ss;
    ss << "UFS Completion Interrupt: UTP_TRANSFER_REQ_INT_STATUS = " << FormatHex32(status_mask);
    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::STORAGE,
        ::emulator::logger::models::LogLevel::INFO,
        ss.str());
}

std::vector<ScsiCommandRecord> UfsTelemetry::GetHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<ScsiCommandRecord>(history_.begin(), history_.end());
}

void UfsTelemetry::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.clear();
}

nlohmann::json UfsTelemetry::ToJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j = nlohmann::json::array();
    for (const auto& cmd : history_) {
        j.push_back(cmd.ToJson());
    }
    return j;
}

void UfsTelemetry::SaveHistoryJson(const std::filesystem::path& file_path) const {
    if (file_path.has_parent_path()) {
        std::filesystem::create_directories(file_path.parent_path());
    }
    std::ofstream ofs(file_path);
    if (ofs.is_open()) {
        ofs << ToJson().dump(2);
    }
}

} // namespace emulator::storage
