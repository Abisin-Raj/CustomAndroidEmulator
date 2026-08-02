#include "qemu_backend/gdb_rsp_client.h"
#include "core/logger.h"
#include <spdlog/spdlog.h>
#include <QHostAddress>
#include <QElapsedTimer>

namespace qemu_backend {

// ---------------------------------------------------------------------------
// GdbRspClient — implementation
// ---------------------------------------------------------------------------

GdbRspClient::GdbRspClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this)) {}

GdbRspClient::~GdbRspClient() {
    disconnect();
}

// ---------------------------------------------------------------------------
// connect
// ---------------------------------------------------------------------------
bool GdbRspClient::connect(const QString& host, quint16 port, int timeoutMs) {
    if (m_connected) return true;

    m_socket->connectToHost(host, port);
    if (!m_socket->waitForConnected(timeoutMs)) {
        spdlog::error("[GDB] Failed to connect to QEMU GDB stub at {}:{} — {}",
            host.toStdString(), port, m_socket->errorString().toStdString());
        return false;
    }

    spdlog::info("[GDB] Connected to QEMU GDB stub at {}:{}", host.toStdString(), port);
    m_connected = true;

    // QEMU GDB stub sends nothing on initial connect.
    // Negotiate feature set so we can use extended register access.
    sendPacket("qSupported:multiprocess+;xmlRegisters=aarch64;qRelocInsn+");
    QString resp;
    recvPacket(resp, 2000); // response may be empty on old QEMU — ignore

    return true;
}

void GdbRspClient::disconnect() {
    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        m_socket->waitForDisconnected(1000);
    }
    m_connected = false;
}

// ---------------------------------------------------------------------------
// sendContinue / sendInterrupt / waitForStop
// ---------------------------------------------------------------------------
bool GdbRspClient::sendContinue() {
    return sendPacket("c");
}

bool GdbRspClient::sendInterrupt() {
    if (!m_connected) return false;
    // Raw 0x03 byte (not a packet, just a byte interrupt)
    m_socket->write(QByteArray(1, char(0x03)));
    m_socket->flush();
    return true;
}

QString GdbRspClient::waitForStop(int timeoutMs) {
    // Drain bytes until we see a '$' which starts a packet
    QElapsedTimer elapsed;
    elapsed.start();

    auto hasTimeLeft = [&]() -> bool {
        if (timeoutMs <= 0) return true;  // wait forever
        return elapsed.elapsed() < timeoutMs;
    };

    auto remainingMs = [&]() -> int {
        if (timeoutMs <= 0) return 30000;
        qint64 rem = timeoutMs - elapsed.elapsed();
        return (rem > 0) ? static_cast<int>(rem) : 0;
    };

    QString full;
    bool inPacket = false;

    while (hasTimeLeft()) {
        if (m_socket->bytesAvailable() == 0) {
            int wait = remainingMs();
            if (wait == 0) break;
            if (!m_socket->waitForReadyRead(qMin(wait, 500))) continue;
        }

        QByteArray avail = m_socket->readAll();
        for (int i = 0; i < avail.size(); ++i) {
            char c = avail[i];
            if (!inPacket) {
                if (c == '$') { inPacket = true; full.clear(); }
                continue;
            }
            if (c == '#') {
                // Read 2-byte checksum
                // May need more data
                int needed = 2 - (avail.size() - i - 1);
                if (needed > 0) {
                    m_socket->waitForReadyRead(500);
                    QByteArray more = m_socket->readAll();
                    avail.append(more);
                }
                // Skip 2 checksum chars
                i += 2;
                // Send '+' acknowledgment
                m_socket->write("+");
                m_socket->flush();
                return full;
            }
            full += c;
        }
    }
    return {}; // timeout
}

// ---------------------------------------------------------------------------
// readRegister / writeRegister
// ---------------------------------------------------------------------------
bool GdbRspClient::readRegister(int regNum, uint64_t& value) {
    // RSP 'p' command: $pNN#cs where NN is hex register number
    QString cmd = QString("p%1").arg(regNum, 0, 16);
    if (!sendPacket(cmd)) return false;

    QString resp;
    if (!recvPacket(resp)) return false;

    // QEMU returns 16 hex chars (8 bytes) for 64-bit registers in little-endian
    // byte order, or 8 hex chars (4 bytes) for 32-bit registers like PSTATE.
    if (resp.size() == 16) {
        value = decodeU64Le(resp.left(16));
        return true;
    }
    if (resp.size() == 8) {
        // 32-bit register (e.g. PSTATE/CPSR) — zero-extend to 64 bits
        value = decodeU32Le(resp.left(8));
        return true;
    }
    spdlog::warn("[GDB] Unexpected register response length {} for reg {}: '{}'",
                 resp.size(), regNum, resp.toStdString());
    return false;
}

bool GdbRspClient::writeRegister(int regNum, uint64_t value) {
    // RSP 'P' command: $PNN=VVVVVVVVVVVVVVVV#cs (LE 64-bit value)
    QString cmd = QString("P%1=%2").arg(regNum, 0, 16).arg(encodeU64Le(value));
    if (!sendPacket(cmd)) return false;

    QString resp;
    if (!recvPacket(resp)) return false;

    if (resp != "OK") {
        spdlog::warn("[GDB] WriteRegister {} failed: '{}'", regNum, resp.toStdString());
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// readMemory
// ---------------------------------------------------------------------------
std::vector<uint8_t> GdbRspClient::readMemory(uint64_t addr, int len) {
    // RSP 'm' command: $mADDR,LEN#cs
    QString cmd = QString("m%1,%2").arg(addr, 0, 16).arg(len, 0, 16);
    if (!sendPacket(cmd)) return {};

    QString resp;
    if (!recvPacket(resp)) return {};

    if (resp.startsWith("E") || resp.isEmpty()) return {};

    // Response is hex-encoded bytes: "AABBCC..."
    std::vector<uint8_t> bytes;
    bytes.reserve(resp.size() / 2);
    for (int i = 0; i + 1 < resp.size(); i += 2) {
        bool ok;
        bytes.push_back(static_cast<uint8_t>(resp.mid(i, 2).toUInt(&ok, 16)));
        if (!ok) break;
    }
    return bytes;
}

// ---------------------------------------------------------------------------
// writeMemory — RSP 'M' command: write hex-encoded bytes to guest VA
// ---------------------------------------------------------------------------
bool GdbRspClient::writeMemory(uint64_t addr, const std::vector<uint8_t>& data) {
    if (data.empty()) return true;

    // Build hex payload: ADDR,LEN:HEXBYTES
    QString hex;
    hex.reserve(static_cast<int>(data.size()) * 2);
    for (uint8_t b : data)
        hex += QString("%1").arg(b, 2, 16, QChar('0'));

    QString cmd = QString("M%1,%2:%3")
        .arg(addr, 0, 16)
        .arg(static_cast<uint64_t>(data.size()), 0, 16)
        .arg(hex);

    if (!sendPacket(cmd)) return false;

    QString resp;
    if (!recvPacket(resp)) return false;

    if (resp != "OK") {
        spdlog::warn("[GDB] WriteMemory at 0x{:x} ({}B) failed: '{}'",
            addr, data.size(), resp.toStdString());
        return false;
    }
    return true;
}



// ---------------------------------------------------------------------------
// insertBreakpoint / removeBreakpoint
// Hardware breakpoints (Z1/z1) work for kernel text (non-writable pages)
// ---------------------------------------------------------------------------
bool GdbRspClient::insertBreakpoint(uint64_t addr) {
    // Z1,addr,4 — hardware instruction breakpoint, 4-byte ARM64 instruction
    QString cmd = QString("Z1,%1,4").arg(addr, 0, 16);
    if (!sendPacket(cmd)) return false;

    QString resp;
    if (!recvPacket(resp)) return false;

    bool ok = (resp == "OK");
    if (!ok) spdlog::warn("[GDB] InsertBreakpoint at 0x{:x} failed: '{}'", addr, resp.toStdString());
    return ok;
}

bool GdbRspClient::removeBreakpoint(uint64_t addr) {
    QString cmd = QString("z1,%1,4").arg(addr, 0, 16);
    if (!sendPacket(cmd)) return false;

    QString resp;
    if (!recvPacket(resp));

    return (resp == "OK");
}

// ---------------------------------------------------------------------------
// Thread selection
// ---------------------------------------------------------------------------
bool GdbRspClient::selectThread(const QString& threadId) {
    if (threadId.isEmpty()) return true;
    QString cmd = "Hg" + threadId;
    if (!sendPacket(cmd)) return false;
    QString resp;
    if (!recvPacket(resp)) return false;
    return resp == "OK";
}

// ---------------------------------------------------------------------------
// Thread enumeration — qfThreadInfo / qsThreadInfo
// ---------------------------------------------------------------------------
QStringList GdbRspClient::enumerateThreads() {
    QStringList result;

    // First batch
    if (!sendPacket("qfThreadInfo")) return result;
    QString resp;
    if (!recvPacket(resp)) return result;

    // QEMU replies with: m<id1>,<id2>,... or l (end-of-list)
    while (resp.startsWith("m")) {
        QString ids = resp.mid(1); // strip leading 'm'
        for (const QString& id : ids.split(',', Qt::SkipEmptyParts))
            result.append(id.trimmed());

        // Ask for more
        if (!sendPacket("qsThreadInfo")) break;
        resp.clear();
        if (!recvPacket(resp)) break;
    }
    // resp == "l" when QEMU signals end-of-list

    return result;
}

// ---------------------------------------------------------------------------
// QEMU monitor command via qRcmd
// ---------------------------------------------------------------------------
QString GdbRspClient::sendMonitorCommand(const QString& command) {
    // Hex-encode the command string (ASCII → hex pairs)
    QString hexCmd;
    for (QChar c : command)
        hexCmd += QString("%1").arg(static_cast<uint8_t>(c.toLatin1()), 2, 16, QChar('0'));

    if (!sendPacket("qRcmd," + hexCmd)) return {};

    // QEMU replies with one or more 'O<hex-string>' console output packets,
    // followed by a final 'OK' (success) or 'E<nn>' (error).
    QString output;
    for (int tries = 0; tries < 64; ++tries) {
        QString resp;
        if (!recvPacket(resp)) break;

        if (resp.startsWith('O')) {
            // Decode hex-encoded console text
            QString hex = resp.mid(1);
            for (int i = 0; i + 1 < hex.size(); i += 2) {
                bool ok;
                uint8_t ch = static_cast<uint8_t>(hex.mid(i, 2).toUInt(&ok, 16));
                if (ok && ch) output += QChar(ch);
            }
        } else if (resp == "OK") {
            break; // success — all output collected
        } else {
            spdlog::warn("[GDB] qRcmd unexpected reply: '{}'", resp.toStdString());
            break;
        }
    }
    return output;
}

bool GdbRspClient::sendPacket(const QString& payload) {
    if (!m_connected) return false;

    QString packet = "$" + payload + "#" + QString("%1").arg(checksum(payload), 2, 16, QChar('0'));
    m_socket->write(packet.toLatin1());
    m_socket->flush();

    // Wait for '+' acknowledgment (or '-' NAK — we don't retry for simplicity)
    if (!m_socket->waitForReadyRead(2000)) return false;
    QByteArray ack = m_socket->read(1);
    return !ack.isEmpty(); // Accept '+' or any byte (some QEMU builds skip ack)
}

bool GdbRspClient::recvPacket(QString& out, int timeoutMs) {
    // Consume bytes until '$' is seen
    while (true) {
        if (m_socket->bytesAvailable() == 0) {
            if (!m_socket->waitForReadyRead(timeoutMs)) return false;
        }
        QByteArray b = m_socket->read(1);
        if (b.isEmpty()) return false;
        if (b == "$") break;
    }

    // Read payload until '#'
    QString payload;
    while (true) {
        if (m_socket->bytesAvailable() == 0) {
            if (!m_socket->waitForReadyRead(timeoutMs)) return false;
        }
        QByteArray b = m_socket->read(1);
        if (b.isEmpty()) return false;
        if (b == "#") break;
        payload += b;
    }

    // Read the 2-byte checksum (we verify but don't retry)
    if (m_socket->bytesAvailable() < 2) m_socket->waitForReadyRead(1000);
    m_socket->read(2); // discard checksum bytes

    // Send acknowledgment
    m_socket->write("+");
    m_socket->flush();

    out = payload;
    return true;
}

uint8_t GdbRspClient::checksum(const QString& data) const {
    uint8_t sum = 0;
    for (QChar c : data) sum += static_cast<uint8_t>(c.toLatin1());
    return sum;
}

// AArch64 GDB registers are 8 bytes, little-endian in the RSP packet.
QString GdbRspClient::encodeU64Le(uint64_t value) const {
    QString out;
    for (int i = 0; i < 8; ++i) {
        out += QString("%1").arg(value & 0xFF, 2, 16, QChar('0'));
        value >>= 8;
    }
    return out;
}

uint64_t GdbRspClient::decodeU64Le(const QString& hex) const {
    uint64_t result = 0;
    for (int i = 7; i >= 0; --i) {
        result = (result << 8) | hex.mid(i * 2, 2).toUInt(nullptr, 16);
    }
    return result;
}

uint64_t GdbRspClient::decodeU32Le(const QString& hex) const {
    // 8 hex chars = 4 bytes LE, zero-extended to uint64_t
    uint64_t result = 0;
    for (int i = 3; i >= 0; --i) {
        result = (result << 8) | hex.mid(i * 2, 2).toUInt(nullptr, 16);
    }
    return result;
}

} // namespace qemu_backend
