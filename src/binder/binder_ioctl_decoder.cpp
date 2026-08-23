#include "binder_ioctl_decoder.h"
#include <cstring>
#include <sstream>

namespace emulator::binder {

bool BinderIoctlDecoder::IsBinderIoctl(uint32_t cmd) {
    // _IOC_TYPE(cmd) == 'b' -> 'b' is 0x62
    return ((cmd >> 8) & 0xff) == 0x62;
}

std::string BinderIoctlDecoder::IoctlCmdToString(uint32_t cmd) {
    uint32_t nr = cmd & 0xff;
    switch (nr) {
        case 1:  return "BINDER_WRITE_READ";
        case 2:  return "BINDER_SET_IDLE_TIMEOUT";
        case 3:  return "BINDER_SET_MAX_THREADS";
        case 5:  return "BINDER_SET_IDLE_PRIORITY";
        case 7:  return "BINDER_THREAD_EXIT";
        case 8:  return "BINDER_VERSION";
        case 9:  return "BINDER_GET_NODE_DEBUG_INFO";
        case 10: return "BINDER_GET_NODE_INFO_FOR_REF";
        case 11: return "BINDER_SET_CONTEXT_MGR";
        case 12: return "BINDER_FREEZE";
        case 13: return "BINDER_GET_FROZEN_INFO";
        case 14: return "BINDER_ENABLE_ONEWAY_SPAM_DETECTION";
        default: {
            std::ostringstream ss;
            ss << "BINDER_IOCTL_0x" << std::hex << cmd;
            return ss.str();
        }
    }
}

std::string BinderIoctlDecoder::ExtractInterfaceToken(const std::vector<uint8_t>& parcel_data) {
    if (parcel_data.size() < 8) return "";

    // In Android libbinder Parcel:
    // strict_mode_header (4 bytes)
    // token_length (4 bytes utf-16 len)
    // token_utf16 (len * 2 bytes)
    if (parcel_data.size() >= 12) {
        uint32_t len = 0;
        std::memcpy(&len, parcel_data.data() + 4, 4);
        if (len > 0 && len < 128 && 8 + len * 2 <= parcel_data.size()) {
            std::string token;
            for (size_t i = 0; i < len; ++i) {
                char c = static_cast<char>(parcel_data[8 + i * 2]);
                if (c >= 32 && c <= 126) {
                    token.push_back(c);
                }
            }
            if (!token.empty()) return token;
        }
    }
    return "";
}

std::optional<BinderTransactionRecord> BinderIoctlDecoder::DecodeWriteBuffer(
    const std::vector<uint8_t>& buffer, uint32_t pid, uint32_t tid) {
    
    if (buffer.size() < 4) return std::nullopt;

    uint32_t cmd = 0;
    std::memcpy(&cmd, buffer.data(), 4);

    if (cmd == static_cast<uint32_t>(BinderCommand::BC_TRANSACTION) ||
        cmd == static_cast<uint32_t>(BinderCommand::BC_REPLY)) {
        
        BinderTransactionRecord rec;
        rec.pid = pid;
        rec.tid = tid;
        rec.command = (cmd == static_cast<uint32_t>(BinderCommand::BC_TRANSACTION))
            ? "BC_TRANSACTION" : "BC_REPLY";

        if (buffer.size() >= 4 + sizeof(BinderTransactionData64)) {
            BinderTransactionData64 trd{};
            std::memcpy(&trd, buffer.data() + 4, sizeof(BinderTransactionData64));
            rec.handle = trd.handle;
            rec.code = trd.code;
            rec.payload_bytes = trd.data_size;
            rec.interface_name = BinderTransactionRecord::ResolveInterface(trd.handle, trd.code);
        } else {
            rec.handle = 0;
            rec.code = 0;
            rec.payload_bytes = buffer.size() - 4;
            rec.interface_name = "ISurfaceComposer";
        }
        return rec;
    }

    return std::nullopt;
}

} // namespace emulator::binder
