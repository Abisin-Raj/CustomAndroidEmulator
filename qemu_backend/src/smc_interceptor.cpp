#include "qemu_backend/smc_interceptor.h"
#include "qemu_backend/gdb_rsp_client.h"
#include "firmware/smc_types.h"
#include "core/logger.h"
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
    quint16 gdbPort,
    uint64_t functionVirtualAddr,
    QObject* parent)
    : QObject(parent)
    , m_dispatcher(dispatcher)
    , m_gdbPort(gdbPort)
    , m_functionAddr(functionVirtualAddr)
    , m_stop(false) {}

// ---------------------------------------------------------------------------
// findSmcInstruction
//
// Reads guest memory in 64-byte chunks from startAddr and scans for the
// 4-byte SMC opcode (0xD4000003 little-endian = bytes 03 00 00 D4).
// ---------------------------------------------------------------------------
uint64_t SmcInterceptorWorker::findSmcInstruction(
    GdbRspClient& gdb, uint64_t startAddr, int scanBytes) {

    constexpr uint8_t smc_bytes[4] = { 0x03, 0x00, 0x00, 0xD4 };

    spdlog::info("[SMC] Scanning 0x{:016x} + {} bytes for smc opcode...",
        startAddr, scanBytes);

    auto mem = gdb.readMemory(startAddr, scanBytes);
    if (mem.empty()) {
        spdlog::error("[SMC] Memory read failed at 0x{:016x}", startAddr);
        return 0;
    }

    for (int i = 0; i + 3 < static_cast<int>(mem.size()); i += 4) {
        if (mem[i]   == smc_bytes[0] && mem[i+1] == smc_bytes[1] &&
            mem[i+2] == smc_bytes[2] && mem[i+3] == smc_bytes[3]) {
            uint64_t smcAddr = startAddr + static_cast<uint64_t>(i);
            spdlog::info("[SMC] Found smc instruction at 0x{:016x} (offset +0x{:x} from base)",
                smcAddr, i);
            return smcAddr;
        }
    }

    spdlog::warn("[SMC] smc opcode not found in {} bytes from 0x{:016x}", scanBytes, startAddr);
    return 0;
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

    // --- 1. Locate the smc instruction in guest memory ---
    // QEMU starts paused (-S). The kernel is loaded but not yet executing.
    // Guest memory is accessible via GDB even before the CPU starts.
    uint64_t smcAddr = findSmcInstruction(gdb, m_functionAddr, SMC_SCAN_BYTES);
    if (smcAddr == 0) {
        spdlog::error("[SMC] Cannot locate smc instruction. Releasing guest without interception.");
        gdb.sendContinue();
        emit interceptorStopped();
        return;
    }

    // --- 2. Set hardware breakpoint at the smc instruction ---
    if (!gdb.insertBreakpoint(smcAddr)) {
        spdlog::error("[SMC] Failed to set hardware breakpoint at 0x{:016x}", smcAddr);
        gdb.sendContinue();
        emit interceptorStopped();
        return;
    }
    spdlog::info("[SMC] Hardware breakpoint set at 0x{:016x}. Releasing guest...", smcAddr);

    // --- 3. Release the guest from -S pause ---
    gdb.sendContinue();

    // --- 4. Main interception loop ---
    while (!m_stop.load()) {
        // Wait indefinitely for a stop reply (breakpoint hit or SIGTERM)
        QString stopReply = gdb.waitForStop(0);
        if (stopReply.isEmpty() || m_stop.load()) break;

        // --- Read current PC to confirm we hit the SMC breakpoint ---
        uint64_t pc = 0;
        if (!gdb.readRegister(GdbRspClient::REG_PC, pc)) {
            spdlog::warn("[SMC] Cannot read PC after stop; continuing...");
            gdb.sendContinue();
            continue;
        }

        if (pc != smcAddr) {
            // Hit a different stop (e.g. SIGTERM, watchdog); just continue
            spdlog::debug("[SMC] Stop at 0x{:016x} (not our SMC at 0x{:016x}); continuing...",
                pc, smcAddr);
            gdb.sendContinue();
            continue;
        }

        // --- Read X0-X6 (SMC function ID and arguments) ---
        firmware::SmcRequest req;
        uint64_t x0 = 0, x1 = 0, x2 = 0, x3 = 0, x4 = 0, x5 = 0, x6 = 0;

        gdb.readRegister(GdbRspClient::REG_X0, x0);
        gdb.readRegister(GdbRspClient::REG_X0 + 1, x1);
        gdb.readRegister(GdbRspClient::REG_X0 + 2, x2);
        gdb.readRegister(GdbRspClient::REG_X0 + 3, x3);
        gdb.readRegister(GdbRspClient::REG_X0 + 4, x4);
        gdb.readRegister(GdbRspClient::REG_X0 + 5, x5);
        gdb.readRegister(GdbRspClient::REG_X0 + 6, x6);

        req.id.raw  = static_cast<uint32_t>(x0 & 0xFFFFFFFF);
        req.args[0] = x1;
        req.args[1] = x2;
        req.args[2] = x3;
        req.args[3] = x4;
        req.args[4] = x5;
        req.args[5] = x6;

        spdlog::info("[SMC] Intercepted {} at PC=0x{:016x}", req.id.toString(), pc);

        // --- Dispatch through firmware compatibility layer ---
        firmware::SmcResult result = m_dispatcher.dispatch(x0, x1, x2, x3, x4, x5, x6);

        // --- Write return value back into X0 ---
        gdb.writeRegister(GdbRspClient::REG_X0, result.x0);

        // --- Advance PC past the 4-byte smc instruction ---
        gdb.writeRegister(GdbRspClient::REG_PC, pc + 4);

        spdlog::info("[SMC] Emulated: {} → X0=0x{:x} handled={} PC now=0x{:016x}",
            req.id.toString(), result.x0, result.handled, pc + 4);

        emit smcIntercepted(
            static_cast<quint64>(x0),
            result.handled,
            static_cast<quint64>(result.x0)
        );

        // --- Resume guest ---
        gdb.sendContinue();
    }

    // --- Cleanup ---
    gdb.removeBreakpoint(smcAddr);
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
    m_worker = new SmcInterceptorWorker(m_dispatcher, m_gdbPort, funcAddr);
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
