#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <cstdint>
#include <vector>

namespace qemu_backend {

// ---------------------------------------------------------------------------
// GdbRspClient — minimal GDB Remote Serial Protocol (RSP) client
//
// Implements exactly the subset of RSP needed to intercept SMC instructions
// in a running QEMU guest:
//   - Connect to QEMU's -gdb tcp::PORT stub
//   - Set / remove hardware breakpoints (Z1 / z1)
//   - Read and write individual AArch64 guest registers
//   - Read guest memory (for SMC opcode scanning)
//   - Send continue / wait for stop reply
//
// All blocking operations use synchronous waitForReadyRead() and are intended
// to be called from a dedicated worker thread.
// ---------------------------------------------------------------------------
class GdbRspClient : public QObject {
    Q_OBJECT
public:
    explicit GdbRspClient(QObject* parent = nullptr);
    ~GdbRspClient() override;

    // Connect to QEMU's GDB stub. Blocks until connected or timeout.
    bool connect(const QString& host, quint16 port, int timeoutMs = 8000);
    void disconnect();
    bool isConnected() const { return m_connected; }

    // --- Execution control ---

    // Release guest from initial -S pause, or resume after a breakpoint.
    bool sendContinue();

    // Pause a running guest (CTRL-C interrupt, 0x03 byte).
    bool sendInterrupt();

    // Block until a stop reply arrives (T/S signal). Returns raw reply.
    // timeoutMs = 0 → wait forever. Returns empty string on error/timeout.
    QString waitForStop(int timeoutMs = 0);

    // --- AArch64 register indices for QEMU's GDB AArch64 target ---
    static constexpr int REG_X0  =  0;
    static constexpr int REG_X1  =  1;
    static constexpr int REG_X2  =  2;
    static constexpr int REG_X3  =  3;
    static constexpr int REG_X4  =  4;
    static constexpr int REG_X5  =  5;
    static constexpr int REG_X6  =  6;
    static constexpr int REG_X7  =  7;
    static constexpr int REG_X8  =  8;   // Indirect result location (AAPCS64 9th arg)
    static constexpr int REG_X30   = 30;  // Link Register (LR)
    static constexpr int REG_SP    = 31;
    static constexpr int REG_PC    = 32;
    static constexpr int REG_PSTATE = 33; // AArch64 PSTATE/CPSR; EL = (pstate >> 2) & 0x3

    // Read / write one 64-bit guest register by GDB register number.
    bool readRegister(int regNum, uint64_t& value);
    bool writeRegister(int regNum, uint64_t value);

    // Read / write a block of guest memory (virtual addresses).
    // readMemory returns bytes read; empty on error.
    // writeMemory returns true on success.
    std::vector<uint8_t> readMemory(uint64_t addr, int len);
    bool                 writeMemory(uint64_t addr, const std::vector<uint8_t>& data);

    // --- Breakpoints (hardware, Z1/z1) ---
    // Hardware breakpoints work even for ROM / non-writable kernel text.
    bool insertBreakpoint(uint64_t addr);
    bool removeBreakpoint(uint64_t addr);

    // --- Thread selection ---
    // Select active thread for general register/memory access (Hg)
    bool selectThread(const QString& threadId);

    // Enumerate all vCPU thread IDs known to QEMU.
    // Uses RSP qfThreadInfo / qsThreadInfo packets.
    // Returns thread IDs as-received (e.g. "p01.01", "p01.02").
    QStringList enumerateThreads();

    // Send a QEMU monitor command via the GDB RSP qRcmd packet.
    // QEMU replies with output via one or more 'O' hex-encoded console packets.
    // Returns the decoded plain-text output, or an empty string on failure.
    QString sendMonitorCommand(const QString& command);

private:
    // Low-level RSP framing helpers
    bool     sendPacket(const QString& payload);
    bool     recvPacket(QString& out, int timeoutMs = 4000);
    uint8_t  checksum(const QString& data) const;
    QString  encodeU64Le(uint64_t value) const;   // 16 hex chars, LE byte order
    uint64_t decodeU64Le(const QString& hex) const;
    uint64_t decodeU32Le(const QString& hex) const; // 8 hex chars, LE byte order → zero-extended

    QTcpSocket* m_socket   = nullptr;
    bool        m_connected = false;
};

} // namespace qemu_backend
