#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace emulator::gdb {

struct InstructionRecord {
    uint64_t pc{0};
    uint32_t opcode{0};
    std::string mnemonic;
    std::string operands;
    std::string comment;

    nlohmann::json ToJson() const;
};

class InstructionTracer {
public:
    explicit InstructionTracer(size_t max_depth = 128);

    void SetMaxDepth(size_t depth);
    size_t GetMaxDepth() const { return max_depth_; }

    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }

    void Record(uint64_t pc, uint32_t opcode = 0,
                const std::string& mnemonic = "",
                const std::string& operands = "",
                const std::string& comment = "");

    void RecordInstruction(const InstructionRecord& record);

    std::vector<InstructionRecord> GetTrace() const;
    void Clear();

    nlohmann::json ToJson(const std::string& bp_name = "", uint64_t pc = 0, const std::string& reason = "breakpoint_hit") const;
    void SaveTraceJson(const std::filesystem::path& file_path,
                       const std::string& bp_name = "",
                       uint64_t pc = 0,
                       const std::string& reason = "breakpoint_hit") const;


    static InstructionRecord DecodeAArch64(uint64_t pc, uint32_t opcode);

private:
    size_t max_depth_{128};
    bool enabled_{false};
    std::deque<InstructionRecord> ring_buffer_;
};

} // namespace emulator::gdb
