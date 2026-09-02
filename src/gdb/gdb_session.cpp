#include "gdb_session.h"
#include "src/logger/api/unified_logger.h"
#include <sstream>

#include <iostream>
#include <chrono>
#include <thread>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#ifdef ERROR
#undef ERROR
#endif
#else

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
using SOCKET = int;
#endif

namespace emulator::gdb {

static void InitSockets() {
#if defined(_WIN32) || defined(_WIN64)
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        initialized = true;
    }
#endif
}

GdbSession::GdbSession() {
    InitSockets();
}

GdbSession::~GdbSession() {
    Disconnect();
}

bool GdbSession::Connect(const std::string& host, uint16_t port) {
    host_ = host;
    port_ = port;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        ::emulator::logger::api::UnifiedLogger::Instance().Log(
            ::emulator::logger::models::Subsystem::GDB,
            ::emulator::logger::models::LogLevel::ERROR,
            "Failed to create TCP socket for GDB connection");
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
#if defined(_WIN32) || defined(_WIN64)
        closesocket(sock);
#else
        close(sock);
#endif
        std::ostringstream ss;
        ss << "Failed to connect to QEMU GDB stub at " << host << ":" << port;
        ::emulator::logger::api::UnifiedLogger::Instance().Log(
            ::emulator::logger::models::Subsystem::GDB,
            ::emulator::logger::models::LogLevel::ERROR,
            ss.str());
        return false;
    }

    socket_handle_ = static_cast<uintptr_t>(sock);
    connected_ = true;

    std::ostringstream ss;
    ss << "Connected to QEMU GDB stub at " << host << ":" << port;
    ::emulator::logger::api::UnifiedLogger::Instance().Log(
        ::emulator::logger::models::Subsystem::GDB,
        ::emulator::logger::models::LogLevel::INFO,
        ss.str());

    // Send no-ack mode command for speed: QStartNoAckMode
    SendRaw(RspDecoder::FormatPacket("QStartNoAckMode"));
    ReceivePacket(1000);

    // Enable QEMU Physical Memory Mode so GDB 'm' commands read guest physical RAM directly
    SendRaw(RspDecoder::FormatPacket("Qqemu.PhyMemMode:1"));
    ReceivePacket(1000);

    return true;
}

void GdbSession::Disconnect() {
    if (connected_) {
        SOCKET sock = static_cast<SOCKET>(socket_handle_);
#if defined(_WIN32) || defined(_WIN64)
        closesocket(sock);
#else
        close(sock);
#endif
        connected_ = false;
        socket_handle_ = static_cast<uintptr_t>(~0ULL);

        ::emulator::logger::api::UnifiedLogger::Instance().Log(
            ::emulator::logger::models::Subsystem::GDB,
            ::emulator::logger::models::LogLevel::INFO,
            "Disconnected from QEMU GDB stub");
    }
}

bool GdbSession::IsConnected() const {
    return connected_;
}

bool GdbSession::SendRaw(const std::string& data) {
    if (!connected_) return false;
    SOCKET sock = static_cast<SOCKET>(socket_handle_);
    int sent = send(sock, data.c_str(), static_cast<int>(data.size()), 0);
    return sent == static_cast<int>(data.size());
}

std::string GdbSession::ReceivePacket(int timeout_ms) {
    if (!connected_) return "";
    SOCKET sock = static_cast<SOCKET>(socket_handle_);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 2000);
    std::string buffer;
    bool in_packet = false;

    while (std::chrono::steady_clock::now() < deadline) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        timeval tv{0, 20000}; // 20ms poll
        int sel = select(static_cast<int>(sock + 1), &read_fds, nullptr, nullptr, &tv);
        if (sel > 0) {
            char c;
            int n = recv(sock, &c, 1, 0);
            if (n <= 0) break;
            if (c == '$') {
                in_packet = true;
                buffer.clear();
            } else if (c == '#' && in_packet) {
                char cs[2];
                recv(sock, cs, 2, 0);
                SendRaw("+");
                return buffer;
            } else if (in_packet) {
                buffer.push_back(c);
            }
        }
    }
    return buffer;
}


bool GdbSession::AddBreakpoint(const std::string& name, uint64_t address, const std::string& desc) {
    breakpoint_mgr_.Add(name, address, desc);

    if (connected_) {
        // Z0 = Software BP, Z1 = Hardware BP
        std::ostringstream ss;
        ss << "Z1," << std::hex << address << ",4";
        SendRaw(RspDecoder::FormatPacket(ss.str()));
        std::string reply = ReceivePacket(1000);
        return reply == "OK";
    }
    return true;
}

bool GdbSession::RemoveBreakpoint(const std::string& name) {
    auto bp = breakpoint_mgr_.FindByName(name);
    if (bp.has_value()) {
        uint64_t addr = bp.value().address;
        breakpoint_mgr_.Remove(name);
        if (connected_) {
            std::ostringstream ss;
            ss << "z1," << std::hex << addr << ",4";
            SendRaw(RspDecoder::FormatPacket(ss.str()));
            ReceivePacket(1000);
        }
        return true;
    }
    return false;
}

bool GdbSession::RemoveBreakpointByAddress(uint64_t address) {
    breakpoint_mgr_.RemoveByAddress(address);
    if (connected_) {
        std::ostringstream ss;
        ss << "z1," << std::hex << address << ",4";
        SendRaw(RspDecoder::FormatPacket(ss.str()));
        ReceivePacket(1000);
    }
    return true;
}

RegisterSnapshot GdbSession::ReadRegisters() {
    if (!connected_) return RegisterSnapshot{};
    SendRaw(RspDecoder::FormatPacket("g"));
    std::string hex = ReceivePacket(1000);
    return RegisterSnapshot::FromRspGPayload(hex);
}

std::vector<uint8_t> GdbSession::ReadMemory(uint64_t address, size_t length) {
    if (!connected_) return {};

    // Interrupt/Pause running target
    SOCKET sock = static_cast<SOCKET>(socket_handle_);
    char c = 0x03;
    send(sock, &c, 1, 0);
    ReceivePacket(1000);

    std::vector<uint8_t> all_bytes;
    all_bytes.reserve(length);
    const size_t chunk_size = 1024;
    size_t curr_len = 0;
    while (curr_len < length) {
        size_t to_read = std::min(length - curr_len, chunk_size);
        std::ostringstream ss;
        ss << "m" << std::hex << (address + curr_len) << "," << to_read;
        SendRaw(RspDecoder::FormatPacket(ss.str()));
        std::string hex = ReceivePacket(1000);
        if (hex.empty() || hex[0] == 'E') break;
        auto chunk = RspDecoder::HexToBytes(hex);
        if (chunk.empty()) break;
        all_bytes.insert(all_bytes.end(), chunk.begin(), chunk.end());
        curr_len += chunk.size();
    }

    // Resume target execution
    SendRaw(RspDecoder::FormatPacket("vCont;c"));
    return all_bytes;
}


std::string GdbSession::ReadString(uint64_t address, size_t max_len) {
    auto bytes = ReadMemory(address, max_len);
    std::string s;
    for (uint8_t b : bytes) {
        if (b == 0) break;
        if (b >= 32 && b <= 126) s.push_back(static_cast<char>(b));
    }
    return s;
}

StopReply GdbSession::Continue() {
    return ContinueUntilStop(30);
}

StopReply GdbSession::ContinueUntilStop(int max_wait_seconds) {
    if (!connected_) return StopReply{};
    SendRaw(RspDecoder::FormatPacket("vCont;c"));

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < max_wait_seconds) {
        std::string packet = ReceivePacket(2000);
        if (!packet.empty() && packet[0] == 'T') {
            auto stop = RspDecoder::ParseStopReply(packet);
            if (stop.pc.has_value()) {
                auto regs = ReadRegisters();
                regs.pc = stop.pc.value();
                if (stop.lr.has_value()) regs.lr = stop.lr.value();
                if (stop.sp.has_value()) regs.sp = stop.sp.value();

                auto bp_info = breakpoint_mgr_.FindByAddress(stop.pc.value());
                if (bp_info.has_value()) {
                    regs.bp_name = bp_info.value().name;
                }

                breakpoint_mgr_.OnHit(stop.pc.value(), regs);
                return stop;
            }
        }
    }
    return StopReply{};
}

bool GdbSession::ContinueAsync() {
    if (!connected_) return false;
    return SendRaw(RspDecoder::FormatPacket("vCont;c"));
}

bool GdbSession::Interrupt() {
    if (!connected_) return false;
    SOCKET sock = static_cast<SOCKET>(socket_handle_);
    char c = 0x03;
    send(sock, &c, 1, 0);
    std::string reply = ReceivePacket(2000);
    return !reply.empty();
}



StopReply GdbSession::Step() {
    if (!connected_) return StopReply{};
    SendRaw(RspDecoder::FormatPacket("vCont;s"));
    std::string packet = ReceivePacket(2000);
    return RspDecoder::ParseStopReply(packet);
}

StopReply GdbSession::StepPastBreakpoint(uint64_t address) {
    // Temporarily remove BP, step 1 instruction, re-insert BP
    std::ostringstream ss_remove;
    ss_remove << "z1," << std::hex << address << ",4";
    SendRaw(RspDecoder::FormatPacket(ss_remove.str()));
    ReceivePacket(500);

    auto stop = Step();

    std::ostringstream ss_add;
    ss_add << "Z1," << std::hex << address << ",4";
    SendRaw(RspDecoder::FormatPacket(ss_add.str()));
    ReceivePacket(500);

    return stop;
}

BreakpointManager& GdbSession::GetBreakpointManager() {
    return breakpoint_mgr_;
}

} // namespace emulator::gdb
