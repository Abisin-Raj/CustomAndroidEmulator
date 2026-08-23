#pragma once
#include "../session/session_manager.h"
#include "../models/log_entry.h"
#include "../models/subsystem.h"
#include "../models/log_level.h"
#include <string>
#include <memory>
#include <sstream>

namespace emulator::logger::api {

class UnifiedLogger {
public:
    static UnifiedLogger& Instance();

    void Initialize(std::filesystem::path root_log_dir = "logger/sessions");
    std::string StartSession(const std::string& rom_path = "",
                             const std::string& kernel_path = "",
                             const std::string& dtb_path = "");
    void EndSession(models::ExitReason reason = models::ExitReason::SUCCESS);

    void Log(models::Subsystem subsys,
             models::LogLevel level,
             std::string message,
             std::optional<uint64_t> pc = std::nullopt,
             std::optional<double> duration_ms = std::nullopt);

    void LogGdbHit(uint64_t pc, const std::string& symbol_or_desc);
    void LogSyscall(uint32_t nr, const std::string& name, double duration_ms, int ret_val);
    void LogBinder(uint32_t code, uint64_t handle, size_t data_size, const std::string& interface_desc);
    void LogPhaseBegin(const std::string& phase_name);
    void LogPhaseEnd(const std::string& phase_name);

    std::shared_ptr<session::SessionManager> GetSessionManager() const;

private:
    UnifiedLogger();
    ~UnifiedLogger() = default;

    UnifiedLogger(const UnifiedLogger&) = delete;
    UnifiedLogger& operator=(const UnifiedLogger&) = delete;

    std::shared_ptr<session::SessionManager> session_manager_;
};

// Global Macros
#define LOG_MSG(subsys, level, msg) \
    ::emulator::logger::api::UnifiedLogger::Instance().Log( \
        ::emulator::logger::models::Subsystem::subsys, \
        ::emulator::logger::models::LogLevel::level, msg)

#define LOG_INFO(subsys, msg)  LOG_MSG(subsys, INFO, msg)
#define LOG_DEBUG(subsys, msg) LOG_MSG(subsys, DEBUG, msg)
#define LOG_WARN(subsys, msg)  LOG_MSG(subsys, WARN, msg)
#define LOG_ERROR(subsys, msg) LOG_MSG(subsys, ERROR, msg)

#define LOG_BOOT(msg)      LOG_INFO(BOOT, msg)
#define LOG_ROM(msg)       LOG_INFO(ROM, msg)
#define LOG_DTB(msg)       LOG_INFO(DTB, msg)
#define LOG_QEMU(msg)      LOG_INFO(QEMU, msg)
#define LOG_STORAGE(msg)   LOG_INFO(STORAGE, msg)
#define LOG_SMC(msg)       LOG_INFO(SMC, msg)
#define LOG_ANDROID(msg)   LOG_INFO(ANDROID, msg)
#define LOG_DISPLAY(msg)   LOG_INFO(DISPLAY, msg)
#define LOG_LAUNCHER(msg)  LOG_INFO(LAUNCHER, msg)

#define LOG_PHASE_BEGIN(phase) \
    ::emulator::logger::api::UnifiedLogger::Instance().LogPhaseBegin(phase)

#define LOG_PHASE_END(phase) \
    ::emulator::logger::api::UnifiedLogger::Instance().LogPhaseEnd(phase)

#define LOG_GDB_HIT(pc, desc) \
    ::emulator::logger::api::UnifiedLogger::Instance().LogGdbHit(pc, desc)

#define LOG_SYSCALL(nr, name, duration_ms, ret_val) \
    ::emulator::logger::api::UnifiedLogger::Instance().LogSyscall(nr, name, duration_ms, ret_val)

#define LOG_BINDER(code, handle, data_size, iface) \
    ::emulator::logger::api::UnifiedLogger::Instance().LogBinder(code, handle, data_size, iface)

} // namespace emulator::logger::api
