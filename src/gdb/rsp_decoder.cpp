#include "rsp_decoder.h"
#include <iomanip>
#include <sstream>

namespace emulator::gdb {

uint8_t RspDecoder::ComputeChecksum(const std::string& data) {
    uint8_t sum = 0;
    for (char c : data) {
        sum += static_cast<uint8_t>(c);
    }
    return sum;
}

std::string RspDecoder::FormatPacket(const std::string& cmd) {
    uint8_t csum = ComputeChecksum(cmd);
    std::ostringstream ss;
    ss << "$" << cmd << "#"
       << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(csum);
    return ss.str();
}

std::string RspDecoder::UnescapePacket(const std::string& raw_packet) {
    if (raw_packet.empty()) return "";
    size_t start = raw_packet.find('$');
    size_t end = raw_packet.find('#');
    if (start == std::string::npos || end == std::string::npos || end < start) {
        return raw_packet;
    }
    return raw_packet.substr(start + 1, end - start - 1);
}

static uint64_t ParseRspHex64(const std::string& hex) {
    if (hex.size() < 16) return 0;
    uint64_t val = 0;
    for (size_t i = 0; i < 8 && i * 2 + 1 < hex.size(); ++i) {
        std::string byte_hex = hex.substr(i * 2, 2);
        uint8_t b = static_cast<uint8_t>(std::stoul(byte_hex, nullptr, 16));
        val |= (static_cast<uint64_t>(b) << (i * 8));
    }
    return val;
}

StopReply RspDecoder::ParseStopReply(const std::string& payload) {
    StopReply reply;
    if (payload.empty() || payload[0] != 'T') return reply;

    if (payload.size() >= 3) {
        reply.signal = std::stoi(payload.substr(1, 2), nullptr, 16);
    }

    // Key-value pairs in T05 packet: e.g. 20:hex;1f:hex;thread:p01.01;
    size_t pos = 3;
    while (pos < payload.size()) {
        size_t colon = payload.find(':', pos);
        if (colon == std::string::npos) break;
        std::string key = payload.substr(pos, colon - pos);

        size_t semi = payload.find(';', colon);
        std::string val = (semi != std::string::npos)
            ? payload.substr(colon + 1, semi - colon - 1)
            : payload.substr(colon + 1);

        // AArch64 GDB register numbers in stop reply:
        // 20 = pc (32), 1f = sp (31), 1e = lr (30)
        if (key == "20" || key == "20") {
            reply.pc = ParseRspHex64(val);
        } else if (key == "1f") {
            reply.sp = ParseRspHex64(val);
        } else if (key == "1e") {
            reply.lr = ParseRspHex64(val);
        } else if (key == "watch" || key == "rwatch" || key == "awatch") {
            reply.is_watchpoint = true;
        }

        if (semi == std::string::npos) break;
        pos = semi + 1;
    }
    return reply;
}

std::vector<uint8_t> RspDecoder::HexToBytes(const std::string& hex_str) {
    std::vector<uint8_t> bytes;
    bytes.reserve(hex_str.size() / 2);
    for (size_t i = 0; i + 1 < hex_str.size(); i += 2) {
        std::string byte_hex = hex_str.substr(i, 2);
        try {
            bytes.push_back(static_cast<uint8_t>(std::stoul(byte_hex, nullptr, 16)));
        } catch (...) {
            break;
        }
    }
    return bytes;
}

std::string RspDecoder::HexToString(const std::string& hex_str) {
    auto bytes = HexToBytes(hex_str);
    std::string s;
    for (uint8_t b : bytes) {
        if (b == 0) break;
        if (b >= 32 && b <= 126) s.push_back(static_cast<char>(b));
    }
    return s;
}

} // namespace emulator::gdb
