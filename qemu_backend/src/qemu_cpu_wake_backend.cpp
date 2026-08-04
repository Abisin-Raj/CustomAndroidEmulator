#include "qemu_backend/qemu_cpu_wake_backend.h"
#include <spdlog/spdlog.h>

namespace qemu_backend {

QemuCpuWakeBackend::QemuCpuWakeBackend(GdbRspClient* gdb)
    : m_gdb(gdb)
{
}

bool QemuCpuWakeBackend::wakeCpu(uint64_t mpidr, uint64_t entryPa, uint64_t contextId) {
    if (!m_gdb || !m_gdb->isConnected()) {
        spdlog::error("[QemuCpuWakeBackend] GDB client unavailable or disconnected.");
        return false;
    }

    uint64_t target_mpidr = (mpidr != 0) ? mpidr : 0x100;
    uint64_t entry_pa     = (entryPa != 0) ? entryPa : 0x80900000ULL;

    // Board-specific MPIDR translation hook:
    // Qualcomm 0x100 / 0x1 -> vCPU index 1 (GDB thread "p01.02")
    // Qualcomm 0x200 / 0x2 -> vCPU index 2 (GDB thread "p01.03")
    int targetVcpuIdx = 1;
    if ((target_mpidr & 0xFF00) == 0x100 || (target_mpidr & 0xFF) == 1) {
        targetVcpuIdx = 1;
    } else if ((target_mpidr & 0xFF00) == 0x200 || (target_mpidr & 0xFF) == 2) {
        targetVcpuIdx = 2;
    } else if ((target_mpidr & 0xFF) > 0 && (target_mpidr & 0xFF) < 8) {
        targetVcpuIdx = static_cast<int>(target_mpidr & 0xFF);
    }

    QString targetThreadId = QString("p01.%1").arg(targetVcpuIdx + 1, 2, 10, QChar('0'));

    spdlog::info("[QemuCpuWakeBackend Phase 6.15A] Invoking CPU Wake Backend Stub...");
    spdlog::info("[QemuCpuWakeBackend Phase 6.15A] Target MPIDR: 0x{:x} -> Target vCPU Thread: {}",
                 target_mpidr, targetThreadId.toStdString());
    spdlog::info("[QemuCpuWakeBackend Phase 6.15A] Entry Point PA: 0x{:016x} Context ID: 0x{:x}",
                 entry_pa, contextId);

    // 1. Select target vCPU thread (vCPU1)
    if (!m_gdb->selectThread(targetThreadId)) {
        spdlog::error("[QemuCpuWakeBackend] Failed to select target thread {}", targetThreadId.toStdString());
        return false;
    }

    // 2. Program target vCPU architectural registers with 4-byte aligned entry address
    uint64_t aligned_entry = entry_pa & ~3ULL;
    m_gdb->writeRegister(GdbRspClient::REG_PC, aligned_entry);
    m_gdb->writeRegister(GdbRspClient::REG_PSTATE, 0x3c5);          // EL1h mode with DAIF masked
    m_gdb->writeRegister(GdbRspClient::REG_X0, contextId);           // X0 = context ID (X3)

    spdlog::info("[QemuCpuWakeBackend Phase 6.15C] Target vCPU State Programmed! PC=0x{:016x} (aligned from 0x{:016x}) PSTATE=0x3c5 X0=0x{:x}",
                 aligned_entry, entry_pa, contextId);

    // 3. Send QEMU execution wake signal to target vCPU
    spdlog::info("[QemuCpuWakeBackend Phase 6.15C] Sending QEMU TCG execution wake signal to vCPU index {} ({})",
                 targetVcpuIdx, targetThreadId.toStdString());
    m_gdb->sendMonitorCommand(QString("cpu %1").arg(targetVcpuIdx));
    m_gdb->sendMonitorCommand("cont");

    // 4. Reselect calling vCPU thread (vCPU0)
    m_gdb->selectThread("p01.01");

    return true;
}

} // namespace qemu_backend
