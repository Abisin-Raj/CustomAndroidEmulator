#include "qemu_backend/smc_interceptor.h"
#include "qemu_backend/gdb_rsp_client.h"
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

    spdlog::info("[SMC] Scanning 0x{:016x} + {} bytes for smc opcodes...",
        startAddr, scanBytes);

    auto mem = gdb.readMemory(startAddr, scanBytes);
    if (mem.empty()) {
        spdlog::error("[SMC] Memory read failed at 0x{:016x}", startAddr);
        return addrs;
    }

    for (int i = 0; i + 3 < static_cast<int>(mem.size()); i += 4) {
        if (mem[i]   == smc_bytes[0] && mem[i+1] == smc_bytes[1] &&
            mem[i+2] == smc_bytes[2] && mem[i+3] == smc_bytes[3]) {
            uint64_t addr = startAddr + static_cast<uint64_t>(i);
            spdlog::info("[SMC] Found smc instruction at 0x{:016x} (offset +0x{:x} from base)",
                addr, i);
            addrs.push_back(addr);
        }
    }

    if (addrs.empty()) {
        spdlog::warn("[SMC] No smc opcodes found in {} bytes from 0x{:016x}", scanBytes, startAddr);
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
    // --- 1. Release the guest from -S pause immediately ---
    // This allows the guest kernel to run its early assembly bootloader,
    // enable the MMU, and enter virtual memory mode.
    spdlog::info("[SMC] Connected. Releasing guest to initialize MMU...");
    gdb.sendContinue();

    // --- 2. Defer breakpoint insertion until MMU is enabled ---
    // We poll and interrupt the guest until we confirm it is executing in virtual memory space.
    std::vector<uint64_t> smcAddrs;
    bool mmuEnabled = false;
    int retries = 0;

    while (!mmuEnabled && retries < 20 && !m_stop.load()) {
        // Sleep 150ms to allow execution progression
        QThread::msleep(150);

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
            if (pc >= 0xffffff8000000000ULL) {
                spdlog::info("[SMC] Guest MMU confirmed active. CPU executing in virtual space at PC=0x{:016x}", pc);
                mmuEnabled = true;
                
                // Scan memory for all smc instructions now that MMU translation is active
                auto foundAddrs = findSmcInstructions(gdb, m_functionAddr, SMC_SCAN_BYTES);
                if (foundAddrs.empty()) {
                    foundAddrs.push_back(m_functionAddr + 12);
                    spdlog::warn("[SMC] Memory scan failed. Falling back to static address: 0x{:016x}", m_functionAddr + 12);
                }
                smcAddrs = foundAddrs;
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

    // --- 3. Set hardware breakpoints at all resolved SMC instructions ---
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
    gdb.sendContinue();

    // --- 4. Main interception loop ---
    bool firstHit = true;
    while (!m_stop.load()) {
        // Wait indefinitely for a stop reply (breakpoint hit or SIGTERM)
        QString stopReply = gdb.waitForStop(0);
        if (stopReply.isEmpty() || m_stop.load()) break;

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

        spdlog::info("[SMC] Intercepted {} at PC=0x{:016x} LR=0x{:016x} SP=0x{:016x} X19=0x{:016x}: "
                     "X1=0x{:x}, X2=0x{:x}, X3=0x{:x}",
            req.id.toString(), pc, lr, sp, x19, x1, x2, x3);



        // ---------------------------------------------------------------
        // On the very first hit: disassemble the 16 bytes following the
        // smc instruction so we can confirm the arm_smccc_smc epilogue
        // (ldr x4, [sp] ; stp x0, x1, [x4] ; stp x2, x3, [x4, #16])
        // is present.  This tells us exactly where smc_res will be written.
        // ---------------------------------------------------------------
        if (firstHit) {
            firstHit = false;
            spdlog::info("[SMC] === First hit — inspecting function instruction stream ===");
            uint64_t funcBase = pc - 12;
            auto postInsns = gdb.readMemory(funcBase, 256);
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

            // Also disassemble the caller's instructions around LR (LR-64 to LR+64)
            if (lr >= 0xffffff8000000000ULL) {
                spdlog::info("[SMC] === Caller instruction stream around LR=0x{:016x} ===", lr);
                uint64_t lrBase = lr - 64;
                auto lrInsns = gdb.readMemory(lrBase, 128);
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

            // Also log the stack top (first 3 quad-words) to see the smc_res pointer
            if (sp != 0) {
                auto stackTop = gdb.readMemory(sp, 24);
                if (stackTop.size() >= 8) {
                    uint64_t ptr0 = 0, ptr1 = 0, ptr2 = 0;
                    for (int i = 7; i >= 0; --i) ptr0 = (ptr0 << 8) | stackTop[i];
                    if (stackTop.size() >= 16)
                        for (int i = 15; i >= 8; --i) ptr1 = (ptr1 << 8) | stackTop[i];
                    if (stackTop.size() >= 24)
                        for (int i = 23; i >= 16; --i) ptr2 = (ptr2 << 8) | stackTop[i];
                    spdlog::info("[SMC]   SP[0]=0x{:016x}  SP[8]=0x{:016x}  SP[16]=0x{:016x}",
                        ptr0, ptr1, ptr2);
                }
            }
            spdlog::info("[SMC] ============================================================");
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
        if (sp != 0) {
            auto spMem = gdb.readMemory(sp, 8);
            if (spMem.size() == 8) {
                // Reconstruct the smc_res pointer (little-endian u64 at SP)
                uint64_t smcResPtr = 0;
                for (int i = 7; i >= 0; --i)
                    smcResPtr = (smcResPtr << 8) | spMem[i];

                if (smcResPtr != 0 && smcResPtr >= 0xffffff8000000000ULL) {
                    // Build the 32-byte arm_smccc_res payload: a0, a1, a2, a3
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
                } else {
                    spdlog::warn("[SMC] SP[0]=0x{:016x} — not a kernel VA; skipping direct patch",
                        smcResPtr);
                }
            }
        }

        // Direct guest-memory patch via register X19 (if X19 holds the res pointer)
        // Layout of struct qcom_scm_res / arm_smccc_res at x19:
        //   offset  0 (x19 + 0):  status / a0 → X0 return value
        //   offset  8 (x19 + 8):  a0           → X1 return value
        //   offset 16 (x19 + 16): a1           → X2 return value
        //   offset 24 (x19 + 24): a2           → X3 return value
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

            uint64_t patchAddr = x19; // struct begins at x19 (status at offset 0)
            bool patched = gdb.writeMemory(patchAddr, payload);
            spdlog::info("[SMC] Direct x19 result patch → 0x{:016x}: status=0x{:x} a0=0x{:x} a1=0x{:x} [{}]",
                patchAddr, result.x0, result.x1, result.x2, patched ? "OK" : "FAILED");
        }

        // --- Verify written registers ---
        uint64_t verify_x0 = 0, verify_x1 = 0, verify_x2 = 0, verify_x3 = 0;
        gdb.readRegister(GdbRspClient::REG_X0,     verify_x0);
        gdb.readRegister(GdbRspClient::REG_X0 + 1, verify_x1);
        gdb.readRegister(GdbRspClient::REG_X0 + 2, verify_x2);
        gdb.readRegister(GdbRspClient::REG_X0 + 3, verify_x3);
        spdlog::info("[SMC] Register verification: X0=0x{:x}, X1=0x{:x}, X2=0x{:x}, X3=0x{:x}",
            verify_x0, verify_x1, verify_x2, verify_x3);

        // --- Advance PC past the 4-byte smc instruction ---
        gdb.writeRegister(GdbRspClient::REG_PC, pc + 4);

        spdlog::info("[SMC] Emulated: {} → X0=0x{:x} handled={} PC now=0x{:016x}",
            req.id.toString(), result.x0, result.handled, pc + 4);

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
