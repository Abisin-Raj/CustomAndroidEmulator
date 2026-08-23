#pragma once
#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

namespace emulator::smc {

struct SmcCallRecord {
    uint64_t pc{0};
    uint32_t opcode{0};
    std::string service_name;
    std::string function_name;

    uint64_t x1{0};
    uint64_t x2{0};
    uint64_t x3{0};
    uint64_t x4{0};

    uint64_t ret_x0{0};
    uint64_t ret_x1{0};
    uint64_t ret_x2{0};
    uint64_t ret_x3{0};

    double latency_us{0.0};
    std::string timestamp;

    nlohmann::json ToJson() const;
    std::string FormatCallLog() const;
    std::string FormatReturnLog() const;

    static void ResolveOpcodeNames(uint32_t opcode, std::string& out_service, std::string& out_func);
};

} // namespace emulator::smc
