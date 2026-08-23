#pragma once
#include "breakpoint_manager.h"
#include "rsp_decoder.h"
#include "register_snapshot.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace emulator::gdb {

class GdbSession {
public:
    GdbSession();
    ~GdbSession();

    bool Connect(const std::string& host = "127.0.0.1", uint16_t port = 1294);
    void Disconnect();
    bool IsConnected() const;

    bool AddBreakpoint(const std::string& name, uint64_t address, const std::string& desc = "");
    bool RemoveBreakpoint(const std::string& name);
    bool RemoveBreakpointByAddress(uint64_t address);

    RegisterSnapshot ReadRegisters();
    std::vector<uint8_t> ReadMemory(uint64_t address, size_t length);
    std::string ReadString(uint64_t address, size_t max_len = 128);

    StopReply Continue();
    StopReply Step();
    StopReply StepPastBreakpoint(uint64_t address);

    BreakpointManager& GetBreakpointManager();

private:
    bool SendRaw(const std::string& data);
    std::string ReceivePacket(int timeout_ms = 5000);

    uintptr_t socket_handle_{static_cast<uintptr_t>(~0ULL)};
    bool connected_{false};
    std::string host_;
    uint16_t port_{1294};

    BreakpointManager breakpoint_mgr_;
};

} // namespace emulator::gdb
