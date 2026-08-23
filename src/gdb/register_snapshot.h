#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace emulator::gdb {

struct RegisterSnapshot {
    uint64_t pc{0};
    uint64_t lr{0};
    uint64_t sp{0};
    uint64_t pstate{0};
    std::vector<uint64_t> x_regs; // x0 - x30
    std::string timestamp;
    std::string bp_name;

    RegisterSnapshot() : x_regs(31, 0) {}

    nlohmann::json ToJson() const;
    void SaveToFile(const std::filesystem::path& file_path) const;
    static RegisterSnapshot FromRspGPayload(const std::string& hex_payload);
};

} // namespace emulator::gdb
