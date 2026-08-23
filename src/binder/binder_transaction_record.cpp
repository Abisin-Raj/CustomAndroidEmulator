#include "binder_transaction_record.h"
#include <iomanip>
#include <sstream>

namespace emulator::binder {

static std::string FormatHex32(uint32_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << val;
    return ss.str();
}

std::string BinderTransactionRecord::ResolveInterface(uint32_t handle, uint32_t code, const std::string& token) {
    if (!token.empty()) return token;

    if (handle == 0) {
        if (code == 1020 || code == 0x3fc) return "ISurfaceComposer";
        if (code == 1) return "IServiceManager::getService";
        if (code == 2) return "IServiceManager::checkService";
        if (code == 3) return "IServiceManager::addService";
        if (code == 4) return "IServiceManager::listServices";
        return "IServiceManager";
    }

    if (code >= 1000 && code <= 1050) {
        return "ISurfaceComposer";
    }

    return "BinderObject_" + std::to_string(handle);
}

nlohmann::json BinderTransactionRecord::ToJson() const {
    nlohmann::json j;
    j["timestamp"] = timestamp;
    j["pid"] = pid;
    j["tid"] = tid;
    j["interface"] = interface_name;
    j["command"] = command;
    j["handle"] = FormatHex32(handle);
    j["code"] = FormatHex32(code);
    j["payload_bytes"] = payload_bytes;
    j["latency_ms"] = latency_ms;
    j["result"] = result;
    return j;
}

std::string BinderTransactionRecord::FormatCommandLog() const {
    std::ostringstream ss;
    ss << command << "\n"
       << "                         Handle : " << FormatHex32(handle) << "\n"
       << "                         Code   : " << FormatHex32(code) << "\n"
       << "                         Target : " << interface_name << "\n"
       << "                         Size   : " << payload_bytes;
    return ss.str();
}

std::string BinderTransactionRecord::FormatReplyLog() const {
    std::ostringstream ss;
    ss << reply_status << " (Latency: " << std::fixed << std::setprecision(1) << latency_ms << " ms)";
    return ss.str();
}

} // namespace emulator::binder
