#include "breakpoint_manager.h"
#include "src/logger/api/unified_logger.h"
#include <sstream>

#include <iomanip>

namespace emulator::gdb {

void BreakpointManager::Add(const std::string& name, uint64_t address, const std::string& description) {
    BreakpointInfo info;
    info.name = name;
    info.address = address;
    info.active = true;
    info.hit_count = 0;
    info.description = description;

    addr_map_[address] = info;
    name_map_[name] = address;

    std::ostringstream ss;
    ss << "Breakpoint added: '" << name << "' at 0x" << std::hex << address;
    if (!description.empty()) ss << " (" << description << ")";
    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::GDB,
        ::emulator::logger::models::LogLevel::INFO,
        ss.str(), address);
}

void BreakpointManager::Remove(const std::string& name) {
    auto it = name_map_.find(name);
    if (it != name_map_.end()) {
        uint64_t addr = it->second;
        addr_map_.erase(addr);
        name_map_.erase(it);

        std::ostringstream ss;
        ss << "Breakpoint removed: '" << name << "' (0x" << std::hex << addr << ")";
        ::emulator::logger::api::UnifiedLogger::Instance().Log(
            ::emulator::logger::models::Subsystem::GDB,
            ::emulator::logger::models::LogLevel::INFO,
            ss.str(), addr);
    }
}

void BreakpointManager::RemoveByAddress(uint64_t address) {
    auto it = addr_map_.find(address);
    if (it != addr_map_.end()) {
        std::string name = it->second.name;
        name_map_.erase(name);
        addr_map_.erase(it);

        std::ostringstream ss;
        ss << "Breakpoint removed: 0x" << std::hex << address << " ('" << name << "')";
        ::emulator::logger::api::UnifiedLogger::Instance().Log(
            ::emulator::logger::models::Subsystem::GDB,
            ::emulator::logger::models::LogLevel::INFO,
            ss.str(), address);
    }
}

void BreakpointManager::Clear() {
    addr_map_.clear();
    name_map_.clear();
}

std::optional<BreakpointInfo> BreakpointManager::FindByAddress(uint64_t address) const {
    auto it = addr_map_.find(address);
    if (it != addr_map_.end()) return it->second;
    return std::nullopt;
}

std::optional<BreakpointInfo> BreakpointManager::FindByName(const std::string& name) const {
    auto it = name_map_.find(name);
    if (it != name_map_.end()) {
        return FindByAddress(it->second);
    }
    return std::nullopt;
}

std::vector<BreakpointInfo> BreakpointManager::GetAll() const {
    std::vector<BreakpointInfo> list;
    list.reserve(addr_map_.size());
    for (const auto& [addr, info] : addr_map_) {
        list.push_back(info);
    }
    return list;
}

void BreakpointManager::OnHit(uint64_t address, const RegisterSnapshot& regs) {
    auto it = addr_map_.find(address);
    std::string bp_name = (it != addr_map_.end()) ? it->second.name : "unnamed_bp";
    if (it != addr_map_.end()) {
        it->second.hit_count++;
    }

    std::ostringstream ss;
    ss << "HIT BP: " << bp_name
       << " | PC: 0x" << std::hex << regs.pc
       << " | LR: 0x" << regs.lr
       << " | SP: 0x" << regs.sp;

    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::GDB,
        ::emulator::logger::models::LogLevel::HIT,
        ss.str(), address);

    RegisterSnapshot regs_copy = regs;
    regs_copy.bp_name = bp_name;

    // Save register snapshot JSON to artifacts/registers/
    auto session_mgr = ::emulator::logger::api::UnifiedLogger::Instance().GetSessionManager();
    if (session_mgr && session_mgr->IsActive()) {
        auto session_dir = session_mgr->GetCurrentSessionDir();
        std::ostringstream fn;
        fn << "bp_" << bp_name << "_0x" << std::hex << address << ".json";
        auto artifact_path = session_dir / "artifacts" / "registers" / fn.str();
        regs_copy.SaveToFile(artifact_path);
    }

    if (hit_callback_) {
        BreakpointInfo info = (it != addr_map_.end()) ? it->second : BreakpointInfo{bp_name, address, true, 1, ""};
        hit_callback_(info, regs_copy);
    }

}

void BreakpointManager::SetHitCallback(BreakpointHitCallback cb) {
    hit_callback_ = std::move(cb);
}

} // namespace emulator::gdb
