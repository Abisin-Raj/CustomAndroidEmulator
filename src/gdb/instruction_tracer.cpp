#include "instruction_tracer.h"
#include <fstream>
#include <iomanip>
#include <sstream>

namespace emulator::gdb {

static std::string FormatHex(uint64_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << val;
    return ss.str();
}

nlohmann::json InstructionRecord::ToJson() const {
    nlohmann::json j;
    j["pc"] = FormatHex(pc);
    if (opcode != 0) {
        std::ostringstream ss;
        ss << "0x" << std::hex << std::setfill('0') << std::setw(8) << opcode;
        j["opcode"] = ss.str();
    }
    j["mnemonic"] = mnemonic;
    j["operands"] = operands;
    if (!comment.empty()) {
        j["comment"] = comment;
    }
    return j;
}


InstructionTracer::InstructionTracer(size_t max_depth)
    : max_depth_(max_depth) {}

void InstructionTracer::SetMaxDepth(size_t depth) {
    max_depth_ = depth;
    while (ring_buffer_.size() > max_depth_) {
        ring_buffer_.pop_front();
    }
}

void InstructionTracer::Record(uint64_t pc, uint32_t opcode,
                               const std::string& mnemonic,
                               const std::string& operands,
                               const std::string& comment) {
    if (!enabled_) return;

    InstructionRecord rec;
    rec.pc = pc;
    rec.opcode = opcode;
    rec.mnemonic = mnemonic;
    rec.operands = operands;
    rec.comment = comment;

    if (rec.mnemonic.empty() && opcode != 0) {
        rec = DecodeAArch64(pc, opcode);
        if (!comment.empty()) rec.comment = comment;
    }

    RecordInstruction(rec);
}

void InstructionTracer::RecordInstruction(const InstructionRecord& record) {
    if (!enabled_) return;

    if (ring_buffer_.size() >= max_depth_) {
        ring_buffer_.pop_front();
    }
    ring_buffer_.push_back(record);
}

std::vector<InstructionRecord> InstructionTracer::GetTrace() const {
    return std::vector<InstructionRecord>(ring_buffer_.begin(), ring_buffer_.end());
}

void InstructionTracer::Clear() {
    ring_buffer_.clear();
}

nlohmann::json InstructionTracer::ToJson(const std::string& bp_name, uint64_t pc, const std::string& reason) const {
    nlohmann::json j;
    j["reason"] = reason.empty() ? "breakpoint_hit" : reason;
    if (!bp_name.empty()) j["breakpoint"] = bp_name;
    if (pc != 0) j["pc"] = FormatHex(pc);

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

    nlohmann::json inst_array = nlohmann::json::array();
    for (const auto& rec : ring_buffer_) {
        inst_array.push_back(rec.ToJson());
    }
    j["last_instructions"] = inst_array;
    return j;
}

void InstructionTracer::SaveTraceJson(const std::filesystem::path& file_path,
                                      const std::string& bp_name,
                                      uint64_t pc,
                                      const std::string& reason) const {
    if (file_path.has_parent_path()) {
        std::filesystem::create_directories(file_path.parent_path());
    }
    std::ofstream ofs(file_path);
    if (ofs.is_open()) {
        ofs << ToJson(bp_name, pc, reason).dump(2);
    }
}


InstructionRecord InstructionTracer::DecodeAArch64(uint64_t pc, uint32_t opcode) {
    InstructionRecord rec;
    rec.pc = pc;
    rec.opcode = opcode;

    // Fast-path decode for common AArch64 instructions
    if (opcode == 0xd65f03c0) {
        rec.mnemonic = "ret";
        rec.operands = "x30";
    } else if (opcode == 0xd503201f) {
        rec.mnemonic = "nop";
        rec.operands = "";
    } else if ((opcode & 0xffe0001f) == 0xd4000001) {
        rec.mnemonic = "svc";
        rec.operands = "#0x" + FormatHex((opcode >> 5) & 0xffff);
    } else if ((opcode & 0xffe0001f) == 0xd4000003) {
        rec.mnemonic = "smc";
        rec.operands = "#0x" + FormatHex((opcode >> 5) & 0xffff);
    } else if ((opcode & 0xfc000000) == 0x94000000) { // BL
        int32_t imm26 = static_cast<int32_t>(opcode & 0x03ffffff);
        if (imm26 & 0x02000000) imm26 |= 0xfc000000;
        uint64_t target = pc + (static_cast<int64_t>(imm26) << 2);
        rec.mnemonic = "bl";
        rec.operands = FormatHex(target);
    } else if ((opcode & 0xfc000000) == 0x14000000) { // B
        int32_t imm26 = static_cast<int32_t>(opcode & 0x03ffffff);
        if (imm26 & 0x02000000) imm26 |= 0xfc000000;
        uint64_t target = pc + (static_cast<int64_t>(imm26) << 2);
        rec.mnemonic = "b";
        rec.operands = FormatHex(target);
    } else if ((opcode & 0x7e000000) == 0x34000000) { // CBZ
        uint32_t rt = opcode & 0x1f;
        int32_t imm19 = static_cast<int32_t>((opcode >> 5) & 0x7ffff);
        if (imm19 & 0x40000) imm19 |= 0xfff80000;
        uint64_t target = pc + (static_cast<int64_t>(imm19) << 2);
        rec.mnemonic = "cbz";
        rec.operands = "x" + std::to_string(rt) + ", " + FormatHex(target);
    } else if ((opcode & 0x7e000000) == 0x35000000) { // CBNZ
        uint32_t rt = opcode & 0x1f;
        int32_t imm19 = static_cast<int32_t>((opcode >> 5) & 0x7ffff);
        if (imm19 & 0x40000) imm19 |= 0xfff80000;
        uint64_t target = pc + (static_cast<int64_t>(imm19) << 2);
        rec.mnemonic = "cbnz";
        rec.operands = "x" + std::to_string(rt) + ", " + FormatHex(target);
    } else if ((opcode & 0x9f000000) == 0x90000000) { // ADRP
        uint32_t rd = opcode & 0x1f;
        rec.mnemonic = "adrp";
        rec.operands = "x" + std::to_string(rd) + ", [page]";
    } else if ((opcode & 0xff000000) == 0x58000000) { // LDR literal
        uint32_t rt = opcode & 0x1f;
        rec.mnemonic = "ldr";
        rec.operands = "x" + std::to_string(rt) + ", [literal]";
    } else {
        rec.mnemonic = "raw_inst";
        rec.operands = FormatHex(opcode);
    }
    return rec;
}

} // namespace emulator::gdb
