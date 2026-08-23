#pragma once
#include "binder_transaction_record.h"
#include <vector>
#include <cstdint>
#include <string>
#include <optional>

namespace emulator::binder {

struct BinderWriteRead64 {
    uint64_t write_size{0};
    uint64_t write_consumed{0};
    uint64_t write_buffer{0};
    uint64_t read_size{0};
    uint64_t read_consumed{0};
    uint64_t read_buffer{0};
};

struct BinderTransactionData64 {
    uint32_t handle{0};
    uint64_t cookie{0};
    uint32_t code{0};
    uint32_t flags{0};
    int32_t sender_pid{0};
    uint32_t sender_euid{0};
    uint64_t data_size{0};
    uint64_t offsets_size{0};
    uint64_t data_buffer{0};
    uint64_t offsets_buffer{0};
};

class BinderIoctlDecoder {
public:
    static bool IsBinderIoctl(uint32_t cmd);
    static std::string IoctlCmdToString(uint32_t cmd);

    static std::optional<BinderTransactionRecord> DecodeWriteBuffer(
        const std::vector<uint8_t>& buffer, uint32_t pid = 0, uint32_t tid = 0);

    static std::string ExtractInterfaceToken(const std::vector<uint8_t>& parcel_data);
};

} // namespace emulator::binder
