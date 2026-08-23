#pragma once
#include "register_snapshot.h"
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace emulator::gdb {

struct StopReply {
    int signal{5}; // SIGTRAP = 5
    std::optional<uint64_t> pc;
    std::optional<uint64_t> sp;
    std::optional<uint64_t> lr;
    uint32_t thread_id{0};
    bool is_watchpoint{false};
};

class RspDecoder {
public:
    static uint8_t ComputeChecksum(const std::string& data);
    static std::string FormatPacket(const std::string& cmd);
    static std::string UnescapePacket(const std::string& raw_packet);

    static StopReply ParseStopReply(const std::string& packet_payload);
    static std::vector<uint8_t> HexToBytes(const std::string& hex_str);
    static std::string HexToString(const std::string& hex_str);
};

} // namespace emulator::gdb
