#include "register_snapshot.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace emulator::gdb {

static std::string FormatHex(uint64_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << val;
    return ss.str();
}

static uint64_t ParseLittleEndianHex64(const std::string& hex16) {
    if (hex16.size() != 16) return 0;
    uint64_t val = 0;
    for (int i = 0; i < 8; ++i) {
        std::string byte_hex = hex16.substr(i * 2, 2);
        uint8_t b = static_cast<uint8_t>(std::stoul(byte_hex, nullptr, 16));
        val |= (static_cast<uint64_t>(b) << (i * 8));
    }
    return val;
}

RegisterSnapshot RegisterSnapshot::FromRspGPayload(const std::string& hex_payload) {
    RegisterSnapshot snap;
    // Each 64-bit register is 16 hex characters in little-endian order
    // In GDB AArch64 'g' packet:
    // x0..x30 (31 * 16 = 496 hex chars)
    // sp (16 hex chars)
    // pc (16 hex chars)
    // pstate (8 hex chars / 16 hex chars)
    size_t offset = 0;
    for (size_t i = 0; i < 31 && offset + 16 <= hex_payload.size(); ++i) {
        snap.x_regs[i] = ParseLittleEndianHex64(hex_payload.substr(offset, 16));
        offset += 16;
    }
    if (offset + 16 <= hex_payload.size()) {
        snap.sp = ParseLittleEndianHex64(hex_payload.substr(offset, 16));
        offset += 16;
    }
    if (offset + 16 <= hex_payload.size()) {
        snap.pc = ParseLittleEndianHex64(hex_payload.substr(offset, 16));
        offset += 16;
    }
    snap.lr = snap.x_regs[30];

    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    snap.timestamp = ss.str();

    return snap;
}

nlohmann::json RegisterSnapshot::ToJson() const {
    nlohmann::json j;
    j["bp_name"] = bp_name;
    
    if (timestamp.empty()) {
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
        j["timestamp"] = ss.str();
    } else {
        j["timestamp"] = timestamp;
    }

    j["pc"] = FormatHex(pc);
    j["lr"] = FormatHex(lr);
    j["sp"] = FormatHex(sp);

    
    nlohmann::json x_obj = nlohmann::json::object();
    for (size_t i = 0; i < x_regs.size(); ++i) {
        std::string reg_name = "x" + std::to_string(i);
        x_obj[reg_name] = FormatHex(x_regs[i]);
    }
    j["registers"] = x_obj;
    return j;
}

void RegisterSnapshot::SaveToFile(const std::filesystem::path& file_path) const {
    if (file_path.has_parent_path()) {
        std::filesystem::create_directories(file_path.parent_path());
    }
    std::ofstream ofs(file_path);
    if (ofs.is_open()) {
        ofs << ToJson().dump(2);
    }
}

} // namespace emulator::gdb
