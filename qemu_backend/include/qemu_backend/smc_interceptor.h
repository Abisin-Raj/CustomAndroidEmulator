#pragma once
#include "firmware/smc_dispatcher.h"
#include "kernel_analysis/symbol_resolver.h"
#include <QObject>
#include <QThread>
#include <atomic>
#include <cstdint>
#include <string>

namespace qemu_backend {

// ---------------------------------------------------------------------------
// SmcInterceptorWorker — runs in a dedicated QThread
//
// Lifecycle:
//   1. Connects to QEMU's GDB stub
//   2. Scans guest memory from the resolved function base to find the
//      first `smc` instruction (opcode 0xD4000003, little-endian)
//   3. Sets a hardware breakpoint at that address
//   4. Sends 'continue' to release the QEMU -S pause
//   5. Each time the breakpoint fires:
//      a. Reads guest X0-X6 registers → SmcRequest
//      b. Calls SmcDispatcher::dispatch()
//      c. Writes result.x0 back into guest X0
//      d. Advances guest PC by 4 (skip the smc instruction)
//      e. Sends 'continue' to resume
//   6. Loops until stop() is called or QEMU exits
// ---------------------------------------------------------------------------
class SmcInterceptorWorker : public QObject {
    Q_OBJECT
public:
    explicit SmcInterceptorWorker(
        firmware::SmcDispatcher& dispatcher,
        quint16 gdbPort,
        uint64_t functionVirtualAddr,
        QObject* parent = nullptr);

public slots:
    void run();
    void stop();

signals:
    void interceptorStarted();
    void interceptorStopped();
    void smcIntercepted(quint64 smcId, bool handled, quint64 returnX0);
    void nextBlockerDetected(const QString& description);

private:
    // Scan at most scanBytes bytes starting from startAddr for SMC opcode.
    // Returns the virtual address of the SMC instruction, or 0 if not found.
    uint64_t findSmcInstruction(class GdbRspClient& gdb,
                                uint64_t startAddr, int scanBytes);

    firmware::SmcDispatcher& m_dispatcher;
    quint16    m_gdbPort;
    uint64_t   m_functionAddr;
    std::atomic<bool> m_stop{false};
};

// ---------------------------------------------------------------------------
// SmcInterceptor — owns the worker thread and provides the public API
// ---------------------------------------------------------------------------
class SmcInterceptor : public QObject {
    Q_OBJECT
public:
    explicit SmcInterceptor(
        firmware::SmcDispatcher& dispatcher,
        const kernel_analysis::SymbolResolver& resolver,
        quint16 gdbPort = 1234,
        QObject* parent = nullptr);

    ~SmcInterceptor() override;

    // Resolve the SCM function address and start the interceptor thread.
    // Returns false if the symbol can't be found in the resolver.
    bool start();
    void stop();
    bool isRunning() const;

signals:
    void interceptorStarted();
    void interceptorStopped();
    void smcIntercepted(quint64 smcId, bool handled, quint64 returnX0);
    void nextBlockerDetected(const QString& description);

private:
    firmware::SmcDispatcher&                 m_dispatcher;
    const kernel_analysis::SymbolResolver&   m_resolver;
    quint16                                  m_gdbPort;
    QThread*                                 m_thread  = nullptr;
    SmcInterceptorWorker*                    m_worker  = nullptr;
};

} // namespace qemu_backend
