#include "scsi_command_record.h"
#include <iomanip>
#include <sstream>
#include <chrono>

namespace emulator::storage {

static std::string FormatHex64(uint64_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << val;
    return ss.str();
}

nlohmann::json ScsiCommandRecord::ToJson() const {
    nlohmann::json j;
    j["opcode"] = opcode_name.empty() ? std::string(ScsiOpcodeToString(opcode)) : opcode_name;
    j["opcode_hex"] = FormatHex64(opcode);
    j["lun"] = lun;
    j["slot"] = slot;
    j["lba"] = lba;
    j["blocks"] = blocks;
    j["dma_address"] = FormatHex64(dma_address);
    j["latency_ms"] = latency_ms;
    j["result"] = result;
    j["timestamp"] = timestamp;
    return j;
}

std::string ScsiCommandRecord::FormatLog() const {
    std::string op = opcode_name.empty() ? std::string(ScsiOpcodeToString(opcode)) : opcode_name;
    std::ostringstream ss;
    ss << op
       << " | LUN:" << static_cast<int>(lun)
       << " | LBA:" << lba
       << " | Blocks:" << blocks
       << " | DMA:" << FormatHex64(dma_address)
       << " | Latency:" << std::fixed << std::setprecision(2) << latency_ms << "ms"
       << " | Result:" << result;
    return ss.str();
}

} // namespace emulator::storage
