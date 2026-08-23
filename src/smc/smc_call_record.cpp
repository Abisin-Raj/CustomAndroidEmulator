#include "smc_call_record.h"
#include <iomanip>
#include <sstream>

namespace emulator::smc {

static std::string FormatHex64(uint64_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << val;
    return ss.str();
}

static std::string FormatHex32(uint32_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << val;
    return ss.str();
}

void SmcCallRecord::ResolveOpcodeNames(uint32_t op, std::string& out_service, std::string& out_func) {
    switch (op) {
        case 0x0200ff01:
            out_service = "QCOM_SIP";
            out_func = "SIP_SVC_UID";
            break;
        case 0xc2000601:
            out_service = "SCM_INFO";
            out_func = "IS_CALL_AVAILABLE";
            break;
        case 0xc2000201:
            out_service = "SCM_SEC_CFG";
            out_func = "RESTORE_SEC_CFG";
            break;
        case 0xc2000501:
            out_service = "SCM_MEM";
            out_func = "MEM_PROTECT";
            break;
        case 0x82000000:
            out_service = "ARM_STD_SVC";
            out_func = "CALL_COUNT";
            break;
        case 0x82000001:
            out_service = "ARM_STD_SVC";
            out_func = "SVC_UID";
            break;
        case 0x82000003:
            out_service = "ARM_STD_SVC";
            out_func = "SVC_VERSION";
            break;
        case 0x84000000:
            out_service = "PSCI";
            out_func = "VERSION";
            break;
        case 0x84000002:
            out_service = "PSCI";
            out_func = "CPU_OFF";
            break;
        case 0x84000003:
            out_service = "PSCI";
            out_func = "CPU_ON";
            break;
        case 0x84000008:
            out_service = "PSCI";
            out_func = "SYSTEM_OFF";
            break;
        case 0x84000009:
            out_service = "PSCI";
            out_func = "SYSTEM_RESET";
            break;
        default:
            out_service = "CUSTOM_SMC";
            out_func = FormatHex32(op);
            break;
    }
}

nlohmann::json SmcCallRecord::ToJson() const {
    nlohmann::json j;
    j["pc"] = FormatHex64(pc);
    j["opcode"] = FormatHex32(opcode);
    j["service"] = service_name;
    j["function"] = function_name;
    
    j["args"] = {
        {"x1", FormatHex64(x1)},
        {"x2", FormatHex64(x2)},
        {"x3", FormatHex64(x3)},
        {"x4", FormatHex64(x4)}
    };

    j["result"] = {
        {"x0", FormatHex64(ret_x0)},
        {"x1", FormatHex64(ret_x1)},
        {"x2", FormatHex64(ret_x2)},
        {"x3", FormatHex64(ret_x3)}
    };

    j["latency_us"] = latency_us;
    j["timestamp"] = timestamp;
    return j;
}

std::string SmcCallRecord::FormatCallLog() const {
    std::ostringstream ss;
    ss << "CALL | PC:" << FormatHex64(pc)
       << " | Opcode:" << FormatHex32(opcode)
       << " [" << service_name << "::" << function_name << "]"
       << " | X1:" << FormatHex64(x1)
       << " | X2:" << FormatHex64(x2)
       << " | X3:" << FormatHex64(x3);
    return ss.str();
}

std::string SmcCallRecord::FormatReturnLog() const {
    std::ostringstream ss;
    std::string ret_status = (ret_x0 == 0) ? "SUCCESS" : FormatHex64(ret_x0);
    ss << "RETURN | X0:" << ret_status
       << " | X1:" << FormatHex64(ret_x1)
       << " | X2:" << FormatHex64(ret_x2)
       << " | Latency:" << std::fixed << std::setprecision(1) << latency_us << "us";
    return ss.str();
}

} // namespace emulator::smc
