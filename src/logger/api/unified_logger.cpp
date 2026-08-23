#include "unified_logger.h"
#include <iomanip>
#include <sstream>

namespace emulator::logger::api {

UnifiedLogger& UnifiedLogger::Instance() {
    static UnifiedLogger instance;
    return instance;
}

UnifiedLogger::UnifiedLogger()
    : session_manager_(std::make_shared<session::SessionManager>()) {}

void UnifiedLogger::Initialize(std::filesystem::path root_log_dir) {
    session_manager_ = std::make_shared<session::SessionManager>(std::move(root_log_dir));
}

std::string UnifiedLogger::StartSession(const std::string& rom_path,
                                        const std::string& kernel_path,
                                        const std::string& dtb_path) {
    return session_manager_->StartSession(rom_path, kernel_path, dtb_path);
}

void UnifiedLogger::EndSession(models::ExitReason reason) {
    session_manager_->EndSession(reason);
}

void UnifiedLogger::Log(models::Subsystem subsys,
                        models::LogLevel level,
                        std::string message,
                        std::optional<uint64_t> pc,
                        std::optional<double> duration_ms) {
    if (!session_manager_->IsActive()) return;

    models::LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.subsystem = subsys;
    entry.level = level;
    entry.message = std::move(message);
    entry.pc = pc;
    entry.duration_ms = duration_ms;

    auto core = session_manager_->GetLoggerCore();
    if (core) {
        core->Log(std::move(entry));
    }
}

void UnifiedLogger::LogGdbHit(uint64_t pc, const std::string& symbol_or_desc) {
    std::ostringstream ss;
    ss << "0x" << std::hex << pc << std::dec << " " << symbol_or_desc;
    Log(models::Subsystem::GDB, models::LogLevel::HIT, ss.str(), pc);
}

void UnifiedLogger::LogSyscall(uint32_t nr, const std::string& name, double duration_ms, int ret_val) {
    std::ostringstream ss;
    ss << "sys_" << name << "(#" << nr << ") -> ret=" << ret_val;
    Log(models::Subsystem::STORAGE, models::LogLevel::INFO, ss.str(), std::nullopt, duration_ms);
}

void UnifiedLogger::LogBinder(uint32_t code, uint64_t handle, size_t data_size, const std::string& interface_desc) {
    std::ostringstream ss;
    ss << "BC_TRANSACTION -> handle=0x" << std::hex << handle
       << ", code=0x" << code << std::dec
       << " [" << interface_desc << "], size=" << data_size;
    Log(models::Subsystem::BINDER, models::LogLevel::INFO, ss.str());
}

void UnifiedLogger::LogPhaseBegin(const std::string& phase_name) {
    Log(models::Subsystem::BOOT, models::LogLevel::PHASE_BEGIN, phase_name);
}

void UnifiedLogger::LogPhaseEnd(const std::string& phase_name) {
    Log(models::Subsystem::BOOT, models::LogLevel::PHASE_END, phase_name);
}

std::shared_ptr<session::SessionManager> UnifiedLogger::GetSessionManager() const {
    return session_manager_;
}

} // namespace emulator::logger::api
