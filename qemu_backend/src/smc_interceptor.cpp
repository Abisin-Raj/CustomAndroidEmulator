#include "qemu_backend/smc_interceptor.h"
#include "qemu_backend/gdb_rsp_client.h"
#include "qemu_backend/qemu_cpu_wake_backend.h"
#include "firmware/smc_types.h"
#include "core/logger.h"
#include "behavior_exploration_tool/bet_system.h"
#include <spdlog/spdlog.h>

namespace qemu_backend {

// AArch64 SMC #0 opcode (little-endian in memory: 03 00 00 D4)
static constexpr uint32_t SMC_OPCODE = 0xD4000003U;

// Name of the QCOM SCM warm-boot function to intercept
static constexpr const char* SCM_WARM_BOOT_SYMBOL = "qcom_scm_set_warm_boot_addr";

// How many bytes to scan forward from the function base for the SMC opcode
static constexpr int SMC_SCAN_BYTES = 256;

// ============================================================================
// SmcInterceptorWorker
// ============================================================================

SmcInterceptorWorker::SmcInterceptorWorker(
    firmware::SmcDispatcher& dispatcher,
    const kernel_analysis::SymbolResolver& resolver,
    quint16 gdbPort,
    uint64_t functionVirtualAddr,
    QObject* parent)
    : QObject(parent)
    , m_dispatcher(dispatcher)
    , m_resolver(resolver)
    , m_gdbPort(gdbPort)
    , m_functionAddr(functionVirtualAddr)
    , m_stop(false) {}

// ---------------------------------------------------------------------------
// findSmcInstruction
//
// Reads guest memory in 64-byte chunks from startAddr and scans for the
// 4-byte SMC opcode (0xD4000003 little-endian = bytes 03 00 00 D4).
// ---------------------------------------------------------------------------
std::vector<uint64_t> SmcInterceptorWorker::findSmcInstructions(
    GdbRspClient& gdb, uint64_t startAddr, int scanBytes) {

    std::vector<uint64_t> addrs;
    constexpr uint8_t smc_bytes[4] = { 0x03, 0x00, 0x00, 0xD4 };

    constexpr uint8_t hvc_bytes[4] = { 0x02, 0x00, 0x00, 0xD4 };

    spdlog::info("[SMC] Scanning 0x{:016x} + {} bytes for smc/hvc opcodes in chunks...",
        startAddr, scanBytes);

    constexpr int chunkSize = 16384; // 16KB chunks to avoid RSP buffer limits
    for (int offset = 0; offset < scanBytes; offset += chunkSize) {
        int bytesToRead = std::min(chunkSize, scanBytes - offset);
        uint64_t currentAddr = startAddr + offset;
        auto mem = gdb.readMemory(currentAddr, bytesToRead);
        if (mem.empty()) {
            continue;
        }

        for (int i = 0; i + 3 < static_cast<int>(mem.size()); i += 4) {
            bool isSmc = (mem[i]   == smc_bytes[0] && mem[i+1] == smc_bytes[1] &&
                          mem[i+2] == smc_bytes[2] && mem[i+3] == smc_bytes[3]);
            bool isHvc = (mem[i]   == hvc_bytes[0] && mem[i+1] == hvc_bytes[1] &&
                          mem[i+2] == hvc_bytes[2] && mem[i+3] == hvc_bytes[3]);

            if (isSmc || isHvc) {
                uint64_t addr = currentAddr + static_cast<uint64_t>(i);
                spdlog::info("[SMC] Found {} instruction at 0x{:016x}", isSmc ? "smc" : "hvc", addr);
                addrs.push_back(addr);
            }
        }
    }

    if (addrs.empty()) {
        spdlog::warn("[SMC] No smc/hvc opcodes found in {} bytes from 0x{:016x}", scanBytes, startAddr);
    } else {
        spdlog::info("[SMC] Scan complete. Found {} smc/hvc instructions.", addrs.size());
    }

    return addrs;
}

// ---------------------------------------------------------------------------
// run — interceptor event loop (called in the worker thread)
// ---------------------------------------------------------------------------
void SmcInterceptorWorker::run() {
    spdlog::info("[SMC] Interceptor thread started. GDB port: {}", m_gdbPort);
    emit interceptorStarted();

    GdbRspClient gdb;
    if (!gdb.connect("127.0.0.1", m_gdbPort)) {
        spdlog::error("[SMC] Cannot connect to QEMU GDB stub on port {}", m_gdbPort);
        emit interceptorStopped();
        return;
    }
    // Patch BOTH inline smc #0 opcodes (physical) before releasing guest from -S pause.
    // qcom_scm_set_warm_boot_addr has two smc #0 instructions:
    //   0x8090a25c  (= 0xffffff800888a25c virtual) — the FIRST smc, fires during PSCI feature probes
    //   0x8090a31c  (= 0xffffff800888a31c virtual) — the SECOND smc, fires during warm-boot addr set
    uint32_t nopOp = 0xd503201f;
    std::vector<uint8_t> nopBytes = {
        static_cast<uint8_t>(nopOp & 0xFF),
        static_cast<uint8_t>((nopOp >> 8) & 0xFF),
        static_cast<uint8_t>((nopOp >> 16) & 0xFF),
        static_cast<uint8_t>((nopOp >> 24) & 0xFF)
    };
    uint32_t movW0Zero = 0x52800000;
    std::vector<uint8_t> movW0Bytes = {
        static_cast<uint8_t>(movW0Zero & 0xFF),
        static_cast<uint8_t>((movW0Zero >> 8) & 0xFF),
        static_cast<uint8_t>((movW0Zero >> 16) & 0xFF),
        static_cast<uint8_t>((movW0Zero >> 24) & 0xFF)
    };
    gdb.writeMemory(0x8090a25cULL, movW0Bytes);   // inline smc #0 -> mov w0, #0
    gdb.writeMemory(0x8090a26cULL, movW0Bytes);   // guard
    gdb.writeMemory(0x8090a31cULL, movW0Bytes);   // second inline smc #0
    gdb.writeMemory(0x8090a32cULL, movW0Bytes);   // guard
    gdb.writeMemory(0x824eab08ULL, nopBytes);     // qcom_boot_secondary b.eq -> NOP
    spdlog::info("[SMC] Startup RAM NOP & mov w0,#0 patches applied at 0x8090a25c + 0x8090a31c + 0x824eab08 (t=0.0s)!");

    std::vector<uint64_t> smcAddrs;
    std::vector<uint64_t> targetedAddrs = {
        0xffffff80080a02b4ULL,         // __arm_smccc_smc
        0xffffff80080a02e0ULL,         // __arm_smccc_hvc
        0xffffff800888a250ULL          // qcom_scm_set_warm_boot_addr (secondary CPU bringup)
    };
    smcAddrs = targetedAddrs;
    for (uint64_t addr : targetedAddrs) {
        gdb.insertBreakpoint(addr);
    }
    spdlog::info("[SMC] Armed all 3 essential hardware breakpoints (under QEMU 4-HW-BP limit) at t=0.0s!");

    // Release the guest from -S pause
    spdlog::info("[SMC] Connected. Releasing guest to initialize MMU...");
    gdb.sendContinue();

    // --- 2. Defer breakpoint insertion until MMU is enabled ---
    // We poll and interrupt the guest until we confirm it is executing in virtual memory space.
    bool mmuEnabled = false;
    int retries = 0;

    while (!mmuEnabled && retries < 50 && !m_stop.load()) {
        // Sleep 5ms for ultra-fast MMU enablement capture
        QThread::msleep(5);


        spdlog::info("[SMC] Interrupting guest to verify memory translation state (attempt {})...", retries + 1);
        if (!gdb.sendInterrupt()) {
            spdlog::warn("[SMC] Failed to send interrupt signal to guest.");
            continue;
        }

        QString stopReply = gdb.waitForStop(1000);
        if (stopReply.isEmpty()) {
            spdlog::warn("[SMC] Guest did not stop after interrupt.");
            continue;
        }

        uint64_t pc = 0;
        if (gdb.readRegister(GdbRspClient::REG_PC, pc)) {
            // --- Phase 6.9: Physical _text entry (0x80080000) before MMU enabled ---
            if (pc == 0x80080000ULL) {
                spdlog::info("[Phase 6.9 Step 1] Hit physical _text entry (0x80080000)! Patching RAM and arming psci_ops.cpu_on breakpoint...");
                uint32_t nopOp = 0xd503201f;
                std::vector<uint8_t> nopBytes = {
                    static_cast<uint8_t>(nopOp & 0xFF),
                    static_cast<uint8_t>((nopOp >> 8) & 0xFF),
                    static_cast<uint8_t>((nopOp >> 16) & 0xFF),
                    static_cast<uint8_t>((nopOp >> 24) & 0xFF)
                };
                gdb.writeMemory(0x8090a25cULL, nopBytes);
                gdb.writeMemory(0x8090a26cULL, nopBytes);
                gdb.writeMemory(0x8090a31cULL, nopBytes);
                gdb.writeMemory(0x8090a32cULL, nopBytes);
                gdb.writeMemory(0x824eab08ULL, nopBytes);
                spdlog::info("[Phase 6.9 Step 1] Patched physical 0x8090a25c + 0x824eab08 to NOP!");
                gdb.insertBreakpoint(0xffffff800a46ab18ULL);
                spdlog::info("[Phase 6.9 Step 1] Armed breakpoint at 0xffffff800a46ab18 (bl psci_ops.cpu_on). Resuming guest...");
                gdb.sendContinue();
                retries = 0;  // reset so loop continues waiting for MMU
                continue;
            }

            if (pc >= 0xffffff8000000000ULL) {

                mmuEnabled = true;
                std::vector<uint64_t> targetedAddrs = {
                    0xffffff80080a02b4ULL,         // __arm_smccc_smc
                    0xffffff80080a02e0ULL,         // __arm_smccc_hvc
                    0xffffff800888a250ULL          // qcom_scm_set_warm_boot_addr (secondary CPU bringup)
                };
                smcAddrs = targetedAddrs;




                // Patch inline smc #0 instructions (0xd4000003) to mov w0, #0 (0x52800000) to return SUCCESS (0)
                uint32_t movW0Zero = 0x52800000;
                std::vector<uint8_t> movW0Bytes = {
                    static_cast<uint8_t>(movW0Zero & 0xFF),
                    static_cast<uint8_t>((movW0Zero >> 8) & 0xFF),
                    static_cast<uint8_t>((movW0Zero >> 16) & 0xFF),
                    static_cast<uint8_t>((movW0Zero >> 24) & 0xFF)
                };
                gdb.writeMemory(0xffffff800888a25cULL, movW0Bytes);
                gdb.writeMemory(0xffffff800888a26cULL, movW0Bytes);
                gdb.writeMemory(0xffffff800888a31cULL, movW0Bytes);
                gdb.writeMemory(0xffffff800888a32cULL, movW0Bytes);
                spdlog::info("[SMC] Patched virtual 0x888a25c + 0x888a31c inline smc #0 to mov w0, #0 (SUCCESS)");

                // Patch b.eq 0xa46ab1c at 0xffffff800a46ab08 in qcom_boot_secondary() to NOP (0xd503201f)
                // so failure of qcom_scm_set_warm_boot_addr() does not jump to panic, allowing fallthrough to bl cpu_psci_cpu_boot (PSCI_CPU_ON).
                uint32_t nopOp = 0xd503201f;
                std::vector<uint8_t> nopBytes = {
                    static_cast<uint8_t>(nopOp & 0xFF),
                    static_cast<uint8_t>((nopOp >> 8) & 0xFF),
                    static_cast<uint8_t>((nopOp >> 16) & 0xFF),
                    static_cast<uint8_t>((nopOp >> 24) & 0xFF)
                };
                gdb.writeMemory(0xffffff800a46ab08ULL, nopBytes);
                smcAddrs = targetedAddrs;
                break;





            } else {
                spdlog::info("[SMC] Guest still in physical execution space (PC=0x{:016x}). Resuming...", pc);
                gdb.sendContinue();
            }
        } else {
            spdlog::warn("[SMC] Failed to read PC register after interrupt. Resuming...");
            gdb.sendContinue();
        }
        retries++;
    }

    if (!mmuEnabled || m_stop.load()) {
        spdlog::error("[SMC] MMU did not enable within timeout. Aborting interceptor.");
        emit interceptorStopped();
        return;
    }

    // --- 3. Set targeted hardware breakpoints ---
    for (uint64_t addr : smcAddrs) {
        if (!gdb.insertBreakpoint(addr)) {
            spdlog::error("[SMC] Failed to arm SMC breakpoint at 0x{:016x}", addr);
            // Clean up any armed breakpoints
            for (uint64_t armedAddr : smcAddrs) {
                gdb.removeBreakpoint(armedAddr);
            }
            gdb.sendContinue();
            emit interceptorStopped();
            return;
        }
        spdlog::info("[SMC] SMC breakpoint successfully armed at 0x{:016x}", addr);
    }

    // --- 3b. Log initial vCPU state (PC + PSTATE + EL) for all threads ---
    {
        QStringList threads = gdb.enumerateThreads();
        spdlog::info("[SMC] QEMU reports {} vCPU thread(s)", threads.size());
        for (const QString& tid : threads) {
            gdb.selectThread(tid);
            uint64_t pc = 0, pstate = 0;
            gdb.readRegister(GdbRspClient::REG_PC,     pc);
            gdb.readRegister(GdbRspClient::REG_PSTATE, pstate);
            int el = static_cast<int>((pstate >> 2) & 0x3);
            spdlog::info("[SMC] vCPU {} initial state: PC=0x{:016x} PSTATE=0x{:08x} EL{}",
                         tid.toStdString(), pc, pstate, el);
        }
        // Restore to default thread so the main loop starts cleanly
        if (!threads.isEmpty()) gdb.selectThread(threads.first());
    }

    // --- 3c. Phase 6.6C — Topology validation complete ---
    // Confirmed: QEMU virt machine assigns flat MPIDRs (0x00..0x07) to vCPUs 0..7.
    gdb.sendContinue();



    // --- 4. Main interception loop ---
    bool firstHit = true;
    while (!m_stop.load()) {
        // Wait indefinitely for a stop reply (breakpoint hit or SIGTERM)
        QString stopReply = gdb.waitForStop(0);
        if (stopReply.isEmpty() || m_stop.load()) break;

        // Parse thread ID (e.g. "T05thread:p1.1;" or "T05thread:1;")
        QString threadId;
        int threadIdx = stopReply.indexOf("thread:");
        if (threadIdx != -1) {
            int start = threadIdx + 7;
            int end = stopReply.indexOf(";", start);
            if (end != -1) {
                threadId = stopReply.mid(start, end - start);
            } else {
                threadId = stopReply.mid(start);
            }
        }
        if (!threadId.isEmpty()) {
            spdlog::info("[SMC] Selected thread/CPU: {}", threadId.toStdString());
            gdb.selectThread(threadId);
        }

        // Log state of ALL vCPUs on every stop so we can observe if CPU1+ ever move
        {
            QStringList allThreads = gdb.enumerateThreads();
            for (const QString& tid : allThreads) {
                if (tid == threadId) continue; // stopped thread logged separately below
                gdb.selectThread(tid);
                uint64_t tpc = 0, tpstate = 0;
                gdb.readRegister(GdbRspClient::REG_PC,     tpc);
                gdb.readRegister(GdbRspClient::REG_PSTATE, tpstate);
                int el = static_cast<int>((tpstate >> 2) & 0x3);
                spdlog::debug("[SMC] vCPU {} observer: PC=0x{:016x} PSTATE=0x{:08x} EL{}",
                              tid.toStdString(), tpc, tpstate, el);
            }
            // Re-select stopped thread for all subsequent register access
            if (!threadId.isEmpty()) {
                gdb.selectThread(threadId);
            } else {
                gdb.selectThread("p01.01");
            }
        }

        // --- Read current PC to confirm we hit one of our SMC breakpoints ---
        uint64_t pc = 0;
        if (!gdb.readRegister(GdbRspClient::REG_PC, pc)) {
            spdlog::warn("[SMC] Cannot read PC after stop; continuing...");
            gdb.sendContinue();
            continue;
        }

        if (std::find(smcAddrs.begin(), smcAddrs.end(), pc) == smcAddrs.end()) {
            // Hit a different stop or unhandled event; just continue
            spdlog::debug("[SMC] Stop at 0x{:016x} (not one of our SMCs); continuing...", pc);
            gdb.sendContinue();
            continue;
        }

        // --- Read X0-X6 (SMC function ID and arguments) ---
        firmware::SmcRequest req;
        uint64_t x0 = 0, x1 = 0, x2 = 0, x3 = 0, x4 = 0, x5 = 0, x6 = 0;

        gdb.readRegister(GdbRspClient::REG_X0,     x0);
        gdb.readRegister(GdbRspClient::REG_X0 + 1, x1);
        gdb.readRegister(GdbRspClient::REG_X0 + 2, x2);
        gdb.readRegister(GdbRspClient::REG_X0 + 3, x3);
        gdb.readRegister(GdbRspClient::REG_X0 + 4, x4);
        gdb.readRegister(GdbRspClient::REG_X0 + 5, x5);
        gdb.readRegister(GdbRspClient::REG_X0 + 6, x6);

        // Phase 6.9: Intercept physical kernel _text entry (0x80080000) -> patch inline smc #0 to NOP and arm 0xffffff800a46ab20
        if (pc == 0x80080000ULL || pc == 0xffffff8008000000ULL) {
            spdlog::info("[Phase 6.9 Step 1] Hit kernel _text entry (PC=0x{:016x})! Applying NOP patch to physical 0x8090a31c...", pc);
            uint32_t nopOp = 0xd503201f;
            std::vector<uint8_t> nopBytes = {
                static_cast<uint8_t>(nopOp & 0xFF),
                static_cast<uint8_t>((nopOp >> 8) & 0xFF),
                static_cast<uint8_t>((nopOp >> 16) & 0xFF),
                static_cast<uint8_t>((nopOp >> 24) & 0xFF)
            };
            // Patch both physical RAM (before MMU) and virtual RAM (after MMU)
            gdb.writeMemory(0x8090a31cULL, nopBytes);
            gdb.writeMemory(0x8090a32cULL, nopBytes);
            spdlog::info("[Phase 6.9 Step 1] Patched physical 0x8090a31c/32c to NOP!");
            gdb.sendContinue();
            continue;
        }

        // --- Intercept qcom_scm_set_warm_boot_addr (0x888a250) or qcom_boot_secondary (0xa46ab00 / 0xa46ab18) ---
        if (pc == 0xffffff800888a250ULL || pc == 0xffffff800a46ab00ULL || pc == 0xffffff800a46ab18ULL) {
            spdlog::info("[PSCI Framework Phase 6.13] === CONFIRMED: Execution reached secondary bringup trigger (PC=0x{:016x}) ===", pc);

            // Remove hardware breakpoint at trigger PC to prevent infinite GDB re-trigger loop
            gdb.removeBreakpoint(pc);

            uint64_t target_mpidr = (x1 != 0) ? x1 : 0x100;
            uint64_t entry_pa     = (x2 != 0) ? x2 : 0x80900000ULL;
            uint64_t context_id   = x3;

            spdlog::info("[PSCI Framework Phase 6.15A] Delegating CPU_ON to ICpuWakeBackend...");
            QemuCpuWakeBackend wakeBackend(&gdb);
            bool wakeSuccess = wakeBackend.wakeCpu(target_mpidr, entry_pa, context_id);

            // Return status to calling vCPU0 based strictly on backend result
            uint64_t psciRet = wakeSuccess ? 0ULL : static_cast<uint64_t>(-1LL);
            gdb.writeRegister(GdbRspClient::REG_X0, psciRet);
            gdb.writeRegister(GdbRspClient::REG_PC, 0xffffff800a46ab1cULL);
            spdlog::info("[PSCI Framework Phase 6.15A] Returning status {} (0=SUCCESS, -1=FAIL) to vCPU0, PC advanced to 0xffffff800a46ab1c", psciRet);

            gdb.sendContinue();

            // --- Phase 7.1 Secondary Boot Code Execution Validation ---
            spdlog::info("[Phase 7.1] Starting 20-instruction single-step execution trace on vCPU1 (p01.02)...");
            gdb.selectThread("p01.02");

            for (int step = 1; step <= 20; ++step) {
                uint64_t pc = 0, pstate = 0, sp = 0, lr = 0, x0 = 0;
                gdb.readRegister(GdbRspClient::REG_PC, pc);
                gdb.readRegister(GdbRspClient::REG_PSTATE, pstate);
                gdb.readRegister(GdbRspClient::REG_SP, sp);
                gdb.readRegister(GdbRspClient::REG_X30, lr);
                gdb.readRegister(GdbRspClient::REG_X0, x0);

                int el = static_cast<int>((pstate >> 2) & 0x3);
                std::string symbolStr = m_resolver.resolveAddress(pc);

                // Read opcode at current PC
                uint32_t insn = 0;
                auto codeBuf = gdb.readMemory(pc, 4);
                if (codeBuf.size() == 4) {
                    insn = codeBuf[0] | (codeBuf[1] << 8) | (codeBuf[2] << 16) | (codeBuf[3] << 24);
                }

                spdlog::info("[Phase 7.1 Step {:02d}/20] PC=0x{:016x} [{}] Insn=0x{:08x} EL{} PSTATE=0x{:08x} SP=0x{:016x} LR=0x{:016x} X0=0x{:x}",
                             step, pc, symbolStr, insn, el, pstate, sp, lr, x0);

                if (!gdb.sendStep()) {
                    spdlog::warn("[Phase 7.1] Step {} failed or encountered exception / halt.", step);
                    break;
                }
            }
            gdb.selectThread("p01.01");

            // --- SMP Validation Probe: Measure vCPU1 and vCPU0 execution over time ---
            spdlog::info("[SMP Validation] Beginning empirical vCPU execution tracking (+1ms, +10ms, +100ms, +500ms)...");
            std::vector<int> sampleDelaysMs = {1, 10, 100, 500};
            for (int delayMs : sampleDelaysMs) {
                QThread::msleep(delayMs);
                if (gdb.sendInterrupt()) {
                    QString sReply = gdb.waitForStop(1000);

                    // Sample vCPU1 (p01.02)
                    gdb.selectThread("p01.02");
                    uint64_t cpu1_pc = 0, cpu1_pstate = 0, cpu1_sp = 0, cpu1_lr = 0, cpu1_x0 = 0;
                    gdb.readRegister(GdbRspClient::REG_PC, cpu1_pc);
                    gdb.readRegister(GdbRspClient::REG_PSTATE, cpu1_pstate);
                    gdb.readRegister(GdbRspClient::REG_SP, cpu1_sp);
                    gdb.readRegister(GdbRspClient::REG_X30, cpu1_lr);
                    gdb.readRegister(GdbRspClient::REG_X0, cpu1_x0);

                    // Sample vCPU0 (p01.01)
                    gdb.selectThread("p01.01");
                    uint64_t cpu0_pc = 0, cpu0_pstate = 0, cpu0_sp = 0, cpu0_lr = 0;
                    gdb.readRegister(GdbRspClient::REG_PC, cpu0_pc);
                    gdb.readRegister(GdbRspClient::REG_PSTATE, cpu0_pstate);
                    gdb.readRegister(GdbRspClient::REG_SP, cpu0_sp);
                    gdb.readRegister(GdbRspClient::REG_X30, cpu0_lr);

                    spdlog::info("[SMP Validation] t=+{}ms | vCPU1: PC=0x{:016x} PSTATE=0x{:x} SP=0x{:016x} LR=0x{:016x} X0=0x{:x} | vCPU0: PC=0x{:016x} SP=0x{:016x}",
                                 delayMs, cpu1_pc, cpu1_pstate, cpu1_sp, cpu1_lr, cpu1_x0, cpu0_pc, cpu0_sp);

                    // Disassemble 4 instructions around CPU1 PC
                    if (cpu1_pc != 0) {
                        std::vector<uint8_t> codeBuf = gdb.readMemory(cpu1_pc, 16);
                        if (!codeBuf.empty()) {
                            for (size_t i = 0; i + 3 < codeBuf.size(); i += 4) {
                                uint32_t insn = codeBuf[i] | (codeBuf[i+1] << 8) | (codeBuf[i+2] << 16) | (codeBuf[i+3] << 24);
                                spdlog::info("[SMP Validation Disasm] vCPU1 [0x{:016x}]: 0x{:08x}", cpu1_pc + i, insn);
                            }
                        }
                    }

                    gdb.sendContinue();
                }
            }

            continue;
        }

        // Read LR (X30), SP and X19 for call-chain and result mapping tracing
        uint64_t lr = 0, sp = 0, x19 = 0;

        gdb.readRegister(GdbRspClient::REG_X30, lr);
        gdb.readRegister(GdbRspClient::REG_SP,  sp);
        gdb.readRegister(GdbRspClient::REG_X0 + 19, x19);

        req.id.raw  = static_cast<uint32_t>(x0 & 0xFFFFFFFF);
        req.args[0] = x1;
        req.args[1] = x2;
        req.args[2] = x3;
        req.args[3] = x4;
        req.args[4] = x5;
        req.args[5] = x6;

        if (req.id.raw == 0xC4000003ULL || req.id.raw == 0x84000003ULL) {
            uint64_t guestMpidr = x1;
            uint64_t qemuMpidr = (guestMpidr >= 0x100) ? (guestMpidr >> 8) : guestMpidr;
            spdlog::info("[PSCI] === PSCI CPU_ON Intercepted (0x{:08x}) ===", req.id.raw);
            spdlog::info("[PSCI]   Guest MPIDR: 0x{:x} -> Translated QEMU MPIDR: 0x{:x} | Entry Point: 0x{:016x}",
                         guestMpidr, qemuMpidr, x2);
            spdlog::info("[PSCI]   Caller PC: 0x{:016x} ({}) | LR: 0x{:016x} ({})",
                         pc, m_resolver.resolveAddress(pc), lr, m_resolver.resolveAddress(lr));

            // Write translated MPIDR back into guest register X1 so QEMU's PSCI handler accepts it
            gdb.writeRegister(GdbRspClient::REG_X0 + 1, qemuMpidr);
            x1 = qemuMpidr;
            req.args[0] = qemuMpidr;
        }


        spdlog::info("[SMC] Intercepted {} at PC=0x{:016x} LR=0x{:016x} SP=0x{:016x} X19=0x{:016x}: "
                     "X1=0x{:x}, X2=0x{:x}, X3=0x{:x}",
            req.id.toString(), pc, lr, sp, x19, x1, x2, x3);

        spdlog::info("[SMC]   Symbol PC: {} | Symbol LR: {}",
            m_resolver.resolveAddress(pc), m_resolver.resolveAddress(lr));






        // ---------------------------------------------------------------
        // On the very first hit: disassemble the 16 bytes following the
        // smc instruction so we can confirm the arm_smccc_smc epilogue
        // (ldr x4, [sp] ; stp x0, x1, [x4] ; stp x2, x3, [x4, #16])
        // is present.  This tells us exactly where smc_res will be written.
        // ---------------------------------------------------------------
        if (firstHit) {
            firstHit = false;
            spdlog::info("[SMC] === Inspect function instruction stream on first hit (increased size) ===");
            uint64_t funcBase = pc - 12;
            auto postInsns = gdb.readMemory(funcBase, 512);
            if (!postInsns.empty()) {
                // Log raw bytes in groups of 4 (one AArch64 instruction each)
                for (int i = 0; i + 3 < static_cast<int>(postInsns.size()); i += 4) {
                    uint32_t insn = static_cast<uint32_t>(postInsns[i])
                                  | (static_cast<uint32_t>(postInsns[i+1]) << 8)
                                  | (static_cast<uint32_t>(postInsns[i+2]) << 16)
                                  | (static_cast<uint32_t>(postInsns[i+3]) << 24);
                    spdlog::info("[SMC]   0x{:016x}: 0x{:08x} ({})",
                        funcBase + i, insn,
                        // Basic AArch64 decode hints
                        (insn & 0xFFE0001F) == 0xF9400000 ? "ldr x*, [sp]" :
                        (insn & 0xFFC003FF) == 0xA9000000 ? "stp x*, x*, [x*]" :
                        (insn & 0xFFFFFC1F) == 0xD65F0000 ? "ret" :
                        (insn & 0xFF000000) == 0x94000000 ? "bl #offset" :
                        "other");
                }
            } else {
                spdlog::warn("[SMC]   Memory read failed at function base — guest may not be in virtual space?");
            }

            // Also disassemble the caller's instructions around LR (LR-64 to LR+192)
            if (lr >= 0xffffff8000000000ULL) {
                std::string callerSym = m_resolver.resolveAddress(lr);
                spdlog::info("[SMC] === Caller instruction stream around LR=0x{:016x} [{}] ===", lr, callerSym);

                uint64_t lrBase = lr - 64;
                auto lrInsns = gdb.readMemory(lrBase, 256);
                if (!lrInsns.empty()) {
                    for (int i = 0; i + 3 < static_cast<int>(lrInsns.size()); i += 4) {
                        uint32_t insn = static_cast<uint32_t>(lrInsns[i])
                                      | (static_cast<uint32_t>(lrInsns[i+1]) << 8)
                                      | (static_cast<uint32_t>(lrInsns[i+2]) << 16)
                                      | (static_cast<uint32_t>(lrInsns[i+3]) << 24);
                        spdlog::info("[SMC]   LR{:+d} 0x{:016x}: 0x{:08x}",
                            i - 64, lrBase + i, insn);
                    }
                }
            }

            // Also log the stack top (first 128 bytes) and unwind kernel VAs
            if (sp != 0) {
                auto stackTop = gdb.readMemory(sp, 128);
                if (!stackTop.empty()) {
                    spdlog::info("[SMC] === Unwinding Stack Frames at SP=0x{:016x} ===", sp);
                    for (size_t i = 0; i + 7 < stackTop.size(); i += 8) {
                        uint64_t val = 0;
                        for (int b = 7; b >= 0; --b) val = (val << 8) | stackTop[i + b];
                        if (val >= 0xffffff8000000000ULL) {
                            std::string sym = m_resolver.resolveAddress(val);
                            spdlog::info("[SMC]   SP+{:+03d} [0x{:016x}] -> 0x{:016x} [{}]",
                                static_cast<int>(i), sp + i, val, sym);
                        }
                    }
                }
            }

            spdlog::info("[SMC] ============================================================");
        }

        // --- Special Logging for SCM Resume Call (0x00000001) ---
        if ((x0 & 0xFFFFFFFFULL) == 0x00000001ULL) {
            spdlog::info("[SMC] === SCM Resume Call Intercepted (0x00000001) ===");
            spdlog::info("[SMC]   X0=0x{:016x} X1=0x{:016x} X2=0x{:016x} X3=0x{:016x}", x0, x1, x2, x3);
            spdlog::info("[SMC]   X4=0x{:016x} X5=0x{:016x} X6=0x{:016x}", x4, x5, x6);
            
            // Check if X2 points to a valid kernel virtual address
            if (x2 >= 0xffffff8000000000ULL) {
                auto x2Mem = gdb.readMemory(x2, 32);
                if (!x2Mem.empty()) {
                    std::string hexDump;
                    for (size_t i = 0; i < x2Mem.size(); ++i) {
                        hexDump += fmt::format("{:02x} ", x2Mem[i]);
                        if ((i + 1) % 8 == 0) hexDump += " ";
                    }
                    spdlog::info("[SMC]   [X2] memory dump: {}", hexDump);
                } else {
                    spdlog::warn("[SMC]   [X2] memory read failed (invalid/unmapped address)");
                }
            }
            spdlog::info("[SMC] ===============================================");
        }



        // --- Dispatch through firmware compatibility layer ---
        firmware::SmcResult result = m_dispatcher.dispatch(x0, x1, x2, x3, x4, x5, x6);


        // Keep a copy of dispatcher's original return values
        firmware::SmcResult dispatcherResult = result;

        // Query Override Engine via BetSystem
        std::string ruleId;
        std::string ruleDesc;
        std::map<std::string, uint64_t> overrideRegs;
        bool hasOverride = bet::BetSystem::instance().checkOverride("SMC", fmt::format("0x{:08x}", x0), ruleId, ruleDesc, overrideRegs);

        if (hasOverride) {
            spdlog::info("[SMC] Override rule '{}' matched: {}", ruleId, ruleDesc);
            
            // Apply registers override from rule
            if (overrideRegs.count("x0")) result.x0 = overrideRegs["x0"];
            if (overrideRegs.count("x1")) result.x1 = overrideRegs["x1"];
            if (overrideRegs.count("x2")) result.x2 = overrideRegs["x2"];
            if (overrideRegs.count("x3")) result.x3 = overrideRegs["x3"];
            
            result.handled = true;

            spdlog::info("[SMC]   Original Dispatcher: X0=0x{:x} X1=0x{:x} X2=0x{:x} X3=0x{:x}",
                dispatcherResult.x0, dispatcherResult.x1, dispatcherResult.x2, dispatcherResult.x3);
            spdlog::info("[SMC]   Override Applied:    X0=0x{:x} X1=0x{:x} X2=0x{:x} X3=0x{:x}",
                result.x0, result.x1, result.x2, result.x3);
        }

        // --- Write return values back into X0-X3 ---
        gdb.writeRegister(GdbRspClient::REG_X0,     result.x0);
        gdb.writeRegister(GdbRspClient::REG_X0 + 1, result.x1);
        gdb.writeRegister(GdbRspClient::REG_X0 + 2, result.x2);
        gdb.writeRegister(GdbRspClient::REG_X0 + 3, result.x3);

        // ---------------------------------------------------------------
        // Direct guest-memory patch of arm_smccc_res:
        //
        // arm_smccc_smc() is called with the smc_res pointer as the 9th
        // argument per AAPCS64.  For a function with ≤8 parameters in
        // registers, the 9th argument lands at [SP+0] before the smc.
        //
        // We dereference SP to get the smc_res pointer, then write our
        // return values directly into the struct's a0/a1/a2/a3 fields.
        // This is the most robust path — it survives even if QEMU's
        // hardware-breakpoint register-write model has edge cases.
        // ---------------------------------------------------------------
        if (result.handled) {
            // Apply memory result patch if SP or X19 holds the smc_res pointer
            if (sp != 0) {
                auto spMem = gdb.readMemory(sp, 8);
                if (spMem.size() == 8) {
                    uint64_t smcResPtr = 0;
                    for (int i = 7; i >= 0; --i)
                        smcResPtr = (smcResPtr << 8) | spMem[i];

                    if (smcResPtr != 0 && smcResPtr >= 0xffffff8000000000ULL) {
                        auto encU64Le = [](uint64_t v) -> std::vector<uint8_t> {
                            std::vector<uint8_t> b(8);
                            for (int i = 0; i < 8; ++i) { b[i] = v & 0xFF; v >>= 8; }
                            return b;
                        };
                        std::vector<uint8_t> payload;
                        auto a0 = encU64Le(result.x0); payload.insert(payload.end(), a0.begin(), a0.end());
                        auto a1 = encU64Le(result.x1); payload.insert(payload.end(), a1.begin(), a1.end());
                        auto a2 = encU64Le(result.x2); payload.insert(payload.end(), a2.begin(), a2.end());
                        auto a3 = encU64Le(result.x3); payload.insert(payload.end(), a3.begin(), a3.end());

                        bool patched = gdb.writeMemory(smcResPtr, payload);
                        spdlog::info("[SMC] Direct smc_res patch → 0x{:016x}: a0=0x{:x} a1=0x{:x} [{}]",
                            smcResPtr, result.x0, result.x1, patched ? "OK" : "FAILED");
                    }
                }
            }

            uint64_t x8 = 0;
            gdb.readRegister(GdbRspClient::REG_X0 + 8, x8);
            if (x8 >= 0xffffff8000000000ULL) {
                auto encU64Le = [](uint64_t v) -> std::vector<uint8_t> {
                    std::vector<uint8_t> b(8);
                    for (int i = 0; i < 8; ++i) { b[i] = v & 0xFF; v >>= 8; }
                    return b;
                };
                std::vector<uint8_t> payload;
                auto a0 = encU64Le(result.x0); payload.insert(payload.end(), a0.begin(), a0.end());
                auto a1 = encU64Le(result.x1); payload.insert(payload.end(), a1.begin(), a1.end());
                auto a2 = encU64Le(result.x2); payload.insert(payload.end(), a2.begin(), a2.end());
                auto a3 = encU64Le(result.x3); payload.insert(payload.end(), a3.begin(), a3.end());

                bool patched = gdb.writeMemory(x8, payload);
                spdlog::info("[SMC] Direct AAPCS64 X8 result patch → 0x{:016x}: status=0x{:x} a0=0x{:x} a1=0x{:x} [{}]",
                    x8, result.x0, result.x1, result.x2, patched ? "OK" : "FAILED");
            }

            if (x19 >= 0xffffff8000000000ULL) {
                auto encU64Le = [](uint64_t v) -> std::vector<uint8_t> {
                    std::vector<uint8_t> b(8);
                    for (int i = 0; i < 8; ++i) { b[i] = v & 0xFF; v >>= 8; }
                    return b;
                };
                std::vector<uint8_t> payload;
                auto a0 = encU64Le(result.x0); payload.insert(payload.end(), a0.begin(), a0.end());
                auto a1 = encU64Le(result.x1); payload.insert(payload.end(), a1.begin(), a1.end());
                auto a2 = encU64Le(result.x2); payload.insert(payload.end(), a2.begin(), a2.end());
                auto a3 = encU64Le(result.x3); payload.insert(payload.end(), a3.begin(), a3.end());

                uint64_t patchAddr = x19;
                bool patched = gdb.writeMemory(patchAddr, payload);
                spdlog::info("[SMC] Direct x19 result patch → 0x{:016x}: status=0x{:x} a0=0x{:x} a1=0x{:x} [{}]",
                    patchAddr, result.x0, result.x1, result.x2, patched ? "OK" : "FAILED");
            }

            // Write register return values
            gdb.writeRegister(GdbRspClient::REG_X0,     result.x0);
            gdb.writeRegister(GdbRspClient::REG_X0 + 1, result.x1);
            gdb.writeRegister(GdbRspClient::REG_X0 + 2, result.x2);
            gdb.writeRegister(GdbRspClient::REG_X0 + 3, result.x3);

            // --- Advance PC past the 4-byte smc/hvc instruction ---
            gdb.writeRegister(GdbRspClient::REG_PC, pc + 4);

            spdlog::info("[SMC] Emulated: {} → X0=0x{:x} handled={} PC now=0x{:016x}",
                req.id.toString(), result.x0, result.handled, pc + 4);

            bool isCpuOn = (req.id.raw == 0xC4000003ULL || req.id.raw == 0x84000003ULL);
            if (isCpuOn) {
                // Phase 6.13 Generic PSCI Compatibility Service: Initialize target vCPU state
                uint64_t target_mpidr = x1;
                uint64_t entry_pa     = x2;
                uint64_t context_id   = x3;

                // Board-specific MPIDR translation hook:
                // Qualcomm 0x100 -> vCPU index 1 (GDB thread "p01.02")
                // Qualcomm 0x200 -> vCPU index 2 (GDB thread "p01.03")
                int targetVcpuIdx = 1;
                if ((target_mpidr & 0xFF00) == 0x100 || (target_mpidr & 0xFF) == 1) {
                    targetVcpuIdx = 1;
                } else if ((target_mpidr & 0xFF00) == 0x200 || (target_mpidr & 0xFF) == 2) {
                    targetVcpuIdx = 2;
                } else if ((target_mpidr & 0xFF) > 0 && (target_mpidr & 0xFF) < 8) {
                    targetVcpuIdx = static_cast<int>(target_mpidr & 0xFF);
                }
                QString targetThreadId = QString("p01.%1").arg(targetVcpuIdx + 1, 2, 10, QChar('0'));

                spdlog::info("[PSCI Framework Phase 6.13] === Generic PSCI CPU_ON Service Invoked ===");
                spdlog::info("[PSCI Framework Phase 6.13] Target MPIDR: 0x{:x} -> Target vCPU Thread: {}", target_mpidr, targetThreadId.toStdString());
                spdlog::info("[PSCI Framework Phase 6.13] Entry Point PA: 0x{:016x} Context ID: 0x{:x}", entry_pa, context_id);

                // 1. Select target vCPU thread
                gdb.selectThread(targetThreadId);

                // 2. Program target vCPU architectural registers
                gdb.writeRegister(GdbRspClient::REG_PC, entry_pa);          // Set PC = entry physical address (0x80900000)
                gdb.writeRegister(GdbRspClient::REG_PSTATE, 0x3c5);          // EL1h mode with DAIF masked
                gdb.writeRegister(GdbRspClient::REG_X0, context_id);         // X0 = context ID (X3)

                spdlog::info("[PSCI Framework Phase 6.13] Target vCPU State Programmed: PC=0x{:016x} PSTATE=0x3c5 X0=0x{:x}", entry_pa, context_id);

                // 3. Reselect calling vCPU thread (vCPU0)
                gdb.selectThread("p01.01");
            }

            // Phase 6.9: Single-step post-warm-boot execution to capture panic string & MMIO accesses
            uint32_t smcCmd = static_cast<uint32_t>(x0 & 0xFFFFFFFFULL);
            if (smcCmd == 0x42000A01ULL) {
                spdlog::info("[Phase 6.9 Tracing] Single-stepping post-warm-boot to capture panic string & MMIO accesses...");
                for (int step = 1; step <= 40; ++step) {
                    gdb.sendStep();
                    uint64_t stepPc = 0, stepX0 = 0, stepX1 = 0, stepX2 = 0;
                    gdb.readRegister(GdbRspClient::REG_PC, stepPc);
                    gdb.readRegister(GdbRspClient::REG_X0, stepX0);
                    gdb.readRegister(GdbRspClient::REG_X0 + 1, stepX1);
                    gdb.readRegister(GdbRspClient::REG_X0 + 2, stepX2);

                    std::string strX0 = "";
                    if (stepX0 >= 0xffffff8000000000ULL) {
                        auto strMem = gdb.readMemory(stepX0, 64);
                        if (!strMem.empty() && std::isprint(strMem[0])) {
                            strX0 = std::string(reinterpret_cast<char*>(strMem.data()), strnlen(reinterpret_cast<char*>(strMem.data()), 64));
                        }
                    }

                    spdlog::info("[Phase 6.9 Step {:02d}] PC=0x{:016x} ({}) X0=0x{:x} {} X1=0x{:x} X2=0x{:x}",
                                 step, stepPc, m_resolver.resolveAddress(stepPc), stepX0, strX0.empty() ? "" : fmt::format("[\"{}\"]", strX0), stepX1, stepX2);
                }
            }





        } else {
            bool isCpuOn = (req.id.raw == 0xC4000003ULL || req.id.raw == 0x84000003ULL);

            if (isCpuOn) {
                // Phase 6.11 Hypothesis A Experiment: Test sendContinue() vs sendStep() for PSCI_CPU_ON
                uint64_t cpu0_pc_before = pc;
                uint64_t cpu0_pstate_before = 0;
                gdb.readRegister(GdbRspClient::REG_PSTATE, cpu0_pstate_before);

                uint64_t cpu1_pc_before = 0, cpu1_pstate_before = 0;
                gdb.selectThread("p01.02");
                gdb.readRegister(GdbRspClient::REG_PC, cpu1_pc_before);
                gdb.readRegister(GdbRspClient::REG_PSTATE, cpu1_pstate_before);
                gdb.selectThread("p01.01");

                spdlog::info("[Phase 6.11 Pre-Call Metrics] PSCI_CPU_ON (0x{:08x}): guest_mpidr=0x{:x} -> translated_mpidr=0x{:x} entry=0x{:016x}",
                    req.id.raw, x1, result.x1, x2);
                spdlog::info("[Phase 6.11 Pre-Call Metrics] vCPU0: PC=0x{:016x} PSTATE=0x{:x}", cpu0_pc_before, cpu0_pstate_before);
                spdlog::info("[Phase 6.11 Pre-Call Metrics] vCPU1: PC=0x{:016x} PSTATE=0x{:x}", cpu1_pc_before, cpu1_pstate_before);

                // Write translated MPIDR 0x1 into X1 before resuming
                if (result.x1 != x1) {
                    spdlog::info("[Phase 6.11 Experiment] Translating X1 (MPIDR): 0x{:x} -> 0x{:x}", x1, result.x1);
                    gdb.writeRegister(GdbRspClient::REG_X0 + 1, result.x1);
                }

                // Remove breakpoint at current PC and execute via sendContinue() to test Hypothesis A
                gdb.removeBreakpoint(pc);
                spdlog::info("[Phase 6.11 Experiment] Resuming QEMU natively via sendContinue() (Hypothesis A test)...");
                gdb.sendContinue();

                // Wait for execution to pause at next breakpoint or timeout
                QString stopMsg = gdb.waitForStop(3000);

                uint64_t cpu0_pc_after = 0, cpu0_pstate_after = 0, post_x0 = 0;
                gdb.selectThread("p01.01");
                gdb.readRegister(GdbRspClient::REG_PC, cpu0_pc_after);
                gdb.readRegister(GdbRspClient::REG_PSTATE, cpu0_pstate_after);
                gdb.readRegister(GdbRspClient::REG_X0, post_x0);

                uint64_t cpu1_pc_after = 0, cpu1_pstate_after = 0;
                gdb.selectThread("p01.02");
                gdb.readRegister(GdbRspClient::REG_PC, cpu1_pc_after);
                gdb.readRegister(GdbRspClient::REG_PSTATE, cpu1_pstate_after);
                gdb.selectThread("p01.01");

                bool cpu1_left_reset = (cpu1_pc_after != 0);
                bool cpu1_executed = (cpu1_pc_after != cpu1_pc_before);

                spdlog::info("[Phase 6.11 Post-Call Metrics] StopReason='{}' Post-X0=0x{:016x} (dec {:d})",
                    stopMsg.toStdString(), post_x0, static_cast<int64_t>(post_x0));
                spdlog::info("[Phase 6.11 Post-Call Metrics] vCPU0: PC=0x{:016x} PSTATE=0x{:x}", cpu0_pc_after, cpu0_pstate_after);
                spdlog::info("[Phase 6.11 Post-Call Metrics] vCPU1: PC=0x{:016x} PSTATE=0x{:x} (LeftReset={} Executed={})",
                    cpu1_pc_after, cpu1_pstate_after, cpu1_left_reset ? "YES" : "NO", cpu1_executed ? "YES" : "NO");

                // Re-arm breakpoint at original PC
                gdb.insertBreakpoint(pc);
            } else {
                // Standard passthrough for non-CPU_ON calls
                if (result.x1 != x1) {
                    spdlog::info("[PSCI] Applying translation to X1 before native QEMU step: 0x{:x} -> 0x{:x}", x1, result.x1);
                    gdb.writeRegister(GdbRspClient::REG_X0 + 1, result.x1);
                }

                spdlog::info("[SMC] Passthrough: {} (Unhandled) → stepping natively for QEMU processing (X0_before=0x{:x})",
                    req.id.toString(), x0);
                gdb.removeBreakpoint(pc);
                gdb.sendStep();
                gdb.insertBreakpoint(pc);

                uint64_t post_x0 = 0, post_x1 = 0, post_x2 = 0, post_x3 = 0;
                gdb.readRegister(GdbRspClient::REG_X0,     post_x0);
                gdb.readRegister(GdbRspClient::REG_X0 + 1, post_x1);
                gdb.readRegister(GdbRspClient::REG_X0 + 2, post_x2);
                gdb.readRegister(GdbRspClient::REG_X0 + 3, post_x3);
                spdlog::info("[SMC]   Post-step Native Register Return: X0=0x{:016x} (dec {:d}) X1=0x{:016x} X2=0x{:016x} X3=0x{:016x}",
                    post_x0, static_cast<int64_t>(post_x0), post_x1, post_x2, post_x3);
            }

            if (req.id.raw == 0x84000009ULL) {
                uint64_t fp = 0;
                gdb.readRegister(GdbRspClient::REG_X0 + 29, fp); // X29 = FP
                spdlog::info("[Phase 6.9 StackUnwind] === AAPCS64 Stack Frame Unwind (Initial FP=0x{:016x}) ===", fp);

                for (int frame = 1; frame <= 16 && fp >= 0xffffff8000000000ULL; ++frame) {
                    auto fpMem = gdb.readMemory(fp, 16);
                    if (fpMem.size() < 16) break;

                    uint64_t nextFp = 0, frameLr = 0;
                    for (int i = 7; i >= 0; --i) nextFp = (nextFp << 8) | fpMem[i];
                    for (int i = 15; i >= 8; --i) frameLr = (frameLr << 8) | fpMem[i];

                    spdlog::info("[Phase 6.9 StackUnwind] Frame {:02d}: FP=0x{:016x} LR=0x{:016x} ({})",
                                 frame, fp, frameLr, m_resolver.resolveAddress(frameLr));
                    fp = nextFp;
                }

                // Disassemble qcom_boot_secondary() after SCM warm-boot return (0xffffff800a46ab00..0a46ab80)
                spdlog::info("[Phase 6.9 Disasm] === Disassembling qcom_boot_secondary() after SCM warm-boot return ===");
                auto postScmMem = gdb.readMemory(0xffffff800a46ab00ULL, 128);
                if (!postScmMem.empty()) {
                    for (size_t i = 0; i + 3 < postScmMem.size(); i += 4) {
                        uint64_t insnAddr = 0xffffff800a46ab00ULL + i;
                        uint32_t insn = static_cast<uint32_t>(postScmMem[i])
                                      | (static_cast<uint32_t>(postScmMem[i+1]) << 8)
                                      | (static_cast<uint32_t>(postScmMem[i+2]) << 16)
                                      | (static_cast<uint32_t>(postScmMem[i+3]) << 24);
                        spdlog::info("[Phase 6.9 Disasm]   0x{:016x}: 0x{:08x}", insnAddr, insn);
                    }
                }

            }








            // Phase 6.7C: Deep caller instruction stream disassembly around LR
            if (lr >= 0xffffff8000000000ULL) {
                spdlog::info("[SMC] === Phase 6.7C Caller Disassembly Stream (LR=0x{:016x}) ===", lr);
                uint64_t lrBase = (lr >= 128) ? (lr - 128) : lr;
                auto lrInsns = gdb.readMemory(lrBase, 256);
                if (!lrInsns.empty()) {
                    for (int i = 0; i + 3 < static_cast<int>(lrInsns.size()); i += 4) {
                        uint64_t insnAddr = lrBase + i;
                        uint32_t insn = static_cast<uint32_t>(lrInsns[i])
                                      | (static_cast<uint32_t>(lrInsns[i+1]) << 8)
                                      | (static_cast<uint32_t>(lrInsns[i+2]) << 16)
                                      | (static_cast<uint32_t>(lrInsns[i+3]) << 24);

                        const char* marker = (insnAddr == lr) ? " <-- [LR]" : "";
                        spdlog::info("[SMC]   0x{:016x}: 0x{:08x}{}", insnAddr, insn, marker);
                    }
                }
            }
        }




        emit smcIntercepted(
            static_cast<quint64>(x0),
            result.handled,
            static_cast<quint64>(result.x0)
        );

        // Record event in Behavior Exploration Tool (BET)
        std::vector<uint64_t> beforeRegs = { x0, x1, x2, x3, x4, x5, x6, 0 };
        std::vector<uint64_t> dispatcherRegs = { dispatcherResult.x0, dispatcherResult.x1, dispatcherResult.x2, dispatcherResult.x3, x4, x5, x6, 0 };
        std::vector<uint64_t> afterRegs = { result.x0, result.x1, result.x2, result.x3, x4, x5, x6, 0 };
        std::map<std::string, std::string> meta = {
            {"symbol", m_resolver.resolveAddress(pc)},
            {"service_name", req.id.toString()}
        };
        bet::BetSystem::instance().registerIntercept("SMC", fmt::format("0x{:08x}", x0), pc, lr, sp, beforeRegs, dispatcherRegs, afterRegs, result.handled, ruleId, meta);

        // --- Resume guest ---
        gdb.sendContinue();



    }


    // --- Cleanup ---
    for (uint64_t addr : smcAddrs) {
        gdb.removeBreakpoint(addr);
    }
    gdb.disconnect();
    spdlog::info("[SMC] Interceptor thread exited.");
    emit interceptorStopped();
}

void SmcInterceptorWorker::stop() {
    m_stop.store(true);
}

// ============================================================================
// SmcInterceptor — public API / thread owner
// ============================================================================

SmcInterceptor::SmcInterceptor(
    firmware::SmcDispatcher& dispatcher,
    const kernel_analysis::SymbolResolver& resolver,
    quint16 gdbPort,
    QObject* parent)
    : QObject(parent)
    , m_dispatcher(dispatcher)
    , m_resolver(resolver)
    , m_gdbPort(gdbPort) {}

SmcInterceptor::~SmcInterceptor() {
    stop();
}

bool SmcInterceptor::start() {
    if (isRunning()) return true;

    // Resolve the warm-boot SCM function address using the loaded System.map
    uint64_t funcAddr = m_resolver.findSymbolAddress(SCM_WARM_BOOT_SYMBOL);
    if (funcAddr == 0) {
        spdlog::error("[SMC] Symbol '{}' not found in resolver. "
                      "Ensure System.map is loaded.", SCM_WARM_BOOT_SYMBOL);
        return false;
    }
    spdlog::info("[SMC] Resolved {} → 0x{:016x}", SCM_WARM_BOOT_SYMBOL, funcAddr);

    // Build worker and thread
    m_thread = new QThread(this);
    m_worker = new SmcInterceptorWorker(m_dispatcher, m_resolver, m_gdbPort, funcAddr);
    m_worker->moveToThread(m_thread);

    // Wire lifecycle signals
    connect(m_thread, &QThread::started,
            m_worker, &SmcInterceptorWorker::run);
    connect(m_worker, &SmcInterceptorWorker::interceptorStarted,
            this,     &SmcInterceptor::interceptorStarted);
    connect(m_worker, &SmcInterceptorWorker::interceptorStopped,
            this,     &SmcInterceptor::interceptorStopped);
    connect(m_worker, &SmcInterceptorWorker::smcIntercepted,
            this,     &SmcInterceptor::smcIntercepted);
    connect(m_worker, &SmcInterceptorWorker::nextBlockerDetected,
            this,     &SmcInterceptor::nextBlockerDetected);
    connect(m_worker, &SmcInterceptorWorker::interceptorStopped,
            m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished,
            m_thread, &QObject::deleteLater);

    m_thread->start();
    return true;
}

void SmcInterceptor::stop() {
    if (m_worker) {
        m_worker->stop();
    }
    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait(3000);
    }
    m_thread = nullptr;
    m_worker = nullptr;
}

bool SmcInterceptor::isRunning() const {
    return m_thread && m_thread->isRunning();
}

} // namespace qemu_backend
