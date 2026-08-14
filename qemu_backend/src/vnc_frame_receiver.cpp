#include "qemu_backend/vnc_frame_receiver.h"
#include "core/logger.h"
#include <cstring>

namespace qemu_backend {

VncFrameReceiver::VncFrameReceiver(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_retryTimer(new QTimer(this))
    , m_pollTimer(new QTimer(this)) {
    
    connect(m_socket, &QTcpSocket::connected, this, &VncFrameReceiver::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &VncFrameReceiver::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &VncFrameReceiver::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &VncFrameReceiver::onSocketError);

    m_retryTimer->setInterval(1000);
    connect(m_retryTimer, &QTimer::timeout, this, [this]() {
        if (m_state == VncState::Connecting || m_state == VncState::Disconnected) {
            LOG_QEMU_INFO("VncFrameReceiver: Retrying connection to QEMU display at {}:{}...", m_host.toStdString(), m_port);
            connectToServer(m_host, m_port);
        }
    });

    m_pollTimer->setInterval(16); // ~60 FPS poll heartbeat
    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        if (m_state == VncState::Connected) {
            requestFramebufferUpdate(true);
        }
    });

    m_stride = m_fbWidth * 4;
    m_framebuffer.resize(m_stride * m_fbHeight, 0);
}

VncFrameReceiver::~VncFrameReceiver() {
    disconnectFromServer();
}

void VncFrameReceiver::connectToServer(const QString& host, quint16 port) {
    m_host = host;
    m_port = port;

    if (m_socket->state() == QAbstractSocket::ConnectedState || m_socket->state() == QAbstractSocket::ConnectingState) {
        return;
    }

    m_state = VncState::Connecting;
    m_readBuffer.clear();
    m_pendingRects = 0;
    m_socket->connectToHost(host, port);
}

void VncFrameReceiver::disconnectFromServer() {
    m_retryTimer->stop();
    m_pollTimer->stop();
    if (m_socket->isOpen()) {
        m_socket->disconnectFromHost();
    }
    m_state = VncState::Disconnected;
    emit connectionStateChanged(false);
}

bool VncFrameReceiver::isConnected() const {
    return m_state == VncState::Connected;
}

void VncFrameReceiver::onSocketConnected() {
    m_retryTimer->stop();
    m_state = VncState::WaitingProtocolVersion;
    LOG_QEMU_INFO("VncFrameReceiver: Connected to QEMU display server at {}:{}", m_host.toStdString(), m_port);
}

void VncFrameReceiver::onSocketDisconnected() {
    LOG_QEMU_WARN("VncFrameReceiver: Disconnected from QEMU display server.");
    m_state = VncState::Disconnected;
    emit connectionStateChanged(false);
}

void VncFrameReceiver::onSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    if (m_state == VncState::Connecting) {
        // Will retry via m_retryTimer
        if (!m_retryTimer->isActive()) {
            m_retryTimer->start();
        }
    } else {
        LOG_QEMU_WARN("VncFrameReceiver: Socket error: {}", m_socket->errorString().toStdString());
    }
}

void VncFrameReceiver::onSocketReadyRead() {
    m_readBuffer.append(m_socket->readAll());

    switch (m_state) {
        case VncState::WaitingProtocolVersion:
            processProtocolVersion();
            break;
        case VncState::WaitingSecurityTypes:
            processSecurityTypes();
            break;
        case VncState::WaitingSecurityResult:
            processSecurityResult();
            break;
        case VncState::WaitingServerInit:
            processServerInit();
            break;
        case VncState::Connected:
            processServerMessages();
            break;
        default:
            break;
    }
}

void VncFrameReceiver::processProtocolVersion() {
    if (m_readBuffer.size() < 12) return;

    QByteArray verBytes = m_readBuffer.left(12);
    m_readBuffer.remove(0, 12);

    LOG_QEMU_INFO("VncFrameReceiver: Server RFB version: {}", verBytes.trimmed().toStdString());

    // Reply with RFB 003.008\n
    m_socket->write("RFB 003.008\n", 12);
    m_state = VncState::WaitingSecurityTypes;

    if (!m_readBuffer.isEmpty()) {
        processSecurityTypes();
    }
}

void VncFrameReceiver::processSecurityTypes() {
    if (m_readBuffer.size() < 1) return;

    uint8_t numTypes = static_cast<uint8_t>(m_readBuffer[0]);
    if (m_readBuffer.size() < 1 + numTypes) return;

    bool supportsNone = false;
    for (int i = 0; i < numTypes; ++i) {
        uint8_t secType = static_cast<uint8_t>(m_readBuffer[1 + i]);
        if (secType == 1) { // None
            supportsNone = true;
        }
    }
    m_readBuffer.remove(0, 1 + numTypes);

    if (!supportsNone) {
        LOG_QEMU_ERROR("VncFrameReceiver: Server does not support security type 'None'");
        disconnectFromServer();
        return;
    }

    // Select security type 1 (None)
    uint8_t chosen = 1;
    m_socket->write(reinterpret_cast<const char*>(&chosen), 1);
    m_state = VncState::WaitingSecurityResult;

    if (!m_readBuffer.isEmpty()) {
        processSecurityResult();
    }
}

void VncFrameReceiver::processSecurityResult() {
    if (m_readBuffer.size() < 4) return;

    uint32_t result = 0;
    std::memcpy(&result, m_readBuffer.constData(), 4);
    m_readBuffer.remove(0, 4);

    if (result != 0) {
        LOG_QEMU_ERROR("VncFrameReceiver: Security handshake failed with code {}", result);
        disconnectFromServer();
        return;
    }

    // Send ClientInit (shared = 1)
    uint8_t shared = 1;
    m_socket->write(reinterpret_cast<const char*>(&shared), 1);
    m_state = VncState::WaitingServerInit;

    if (!m_readBuffer.isEmpty()) {
        processServerInit();
    }
}

void VncFrameReceiver::processServerInit() {
    if (m_readBuffer.size() < 24) return;

    const char* ptr = m_readBuffer.constData();
    uint16_t fbW = (static_cast<uint8_t>(ptr[0]) << 8) | static_cast<uint8_t>(ptr[1]);
    uint16_t fbH = (static_cast<uint8_t>(ptr[2]) << 8) | static_cast<uint8_t>(ptr[3]);

    uint32_t nameLen = (static_cast<uint8_t>(ptr[20]) << 24) |
                       (static_cast<uint8_t>(ptr[21]) << 16) |
                       (static_cast<uint8_t>(ptr[22]) << 8) |
                       static_cast<uint8_t>(ptr[23]);

    if (m_readBuffer.size() < 24 + static_cast<int>(nameLen)) return;

    std::string serverName(ptr + 24, nameLen);
    m_readBuffer.remove(0, 24 + nameLen);

    m_fbWidth = fbW > 0 ? fbW : 1080;
    m_fbHeight = fbH > 0 ? fbH : 1920;
    m_stride = m_fbWidth * 4;
    m_framebuffer.resize(m_stride * m_fbHeight, 0);

    LOG_QEMU_INFO("VncFrameReceiver: Server initialized '{}' [{}x{}]", serverName, m_fbWidth, m_fbHeight);

    // Send SetPixelFormat (32-bit RGBA)
    sendPixelFormat();

    // Send SetEncodings (Raw and DesktopSize)
    sendEncodings();

    m_state = VncState::Connected;
    emit connectionStateChanged(true);

    // Request initial full framebuffer update
    requestFramebufferUpdate(false);
    m_pollTimer->start();

    if (!m_readBuffer.isEmpty()) {
        processServerMessages();
    }
}

void VncFrameReceiver::sendPixelFormat() {
    uint8_t msg[20] = {0};
    msg[0] = 0; // SetPixelFormat message type
    // 3 bytes padding (1, 2, 3)
    msg[4] = 32; // bits-per-pixel
    msg[5] = 24; // depth
    msg[6] = 0;  // big-endian-flag (0 = little endian)
    msg[7] = 1;  // true-colour-flag

    // red-max = 255 (big endian uint16)
    msg[8] = 0x00; msg[9] = 0xFF;
    // green-max = 255
    msg[10] = 0x00; msg[11] = 0xFF;
    // blue-max = 255
    msg[12] = 0x00; msg[13] = 0xFF;

    // red-shift = 0, green-shift = 8, blue-shift = 16 (for RGBA memory format)
    msg[14] = 0;
    msg[15] = 8;
    msg[16] = 16;

    m_socket->write(reinterpret_cast<const char*>(msg), 20);
}

void VncFrameReceiver::sendEncodings() {
    uint8_t msg[12] = {0};
    msg[0] = 2; // SetEncodings message type
    msg[1] = 0; // padding
    msg[2] = 0; msg[3] = 2; // number of encodings = 2

    // Encoding 0: Raw (0x00000000)
    msg[4] = 0; msg[5] = 0; msg[6] = 0; msg[7] = 0;

    // Encoding 1: DesktopSize (-223 = 0xFFFFFF21)
    msg[8] = 0xFF; msg[9] = 0xFF; msg[10] = 0xFF; msg[11] = 0x21;

    m_socket->write(reinterpret_cast<const char*>(msg), 12);
}

void VncFrameReceiver::requestFramebufferUpdate(bool incremental) {
    if (m_state != VncState::Connected) return;

    uint8_t msg[10] = {0};
    msg[0] = 3; // FramebufferUpdateRequest message type
    msg[1] = incremental ? 1 : 0;
    msg[2] = 0; msg[3] = 0; // x = 0
    msg[4] = 0; msg[5] = 0; // y = 0
    msg[6] = static_cast<uint8_t>((m_fbWidth >> 8) & 0xFF);
    msg[7] = static_cast<uint8_t>(m_fbWidth & 0xFF);
    msg[8] = static_cast<uint8_t>((m_fbHeight >> 8) & 0xFF);
    msg[9] = static_cast<uint8_t>(m_fbHeight & 0xFF);

    m_socket->write(reinterpret_cast<const char*>(msg), 10);
}

void VncFrameReceiver::processServerMessages() {
    while (!m_readBuffer.isEmpty()) {
        if (m_pendingRects > 0) {
            if (m_readBuffer.size() < 12) return;
            const uint8_t* p = reinterpret_cast<const uint8_t*>(m_readBuffer.constData());
            uint16_t rx = (p[0] << 8) | p[1];
            uint16_t ry = (p[2] << 8) | p[3];
            uint16_t rw = (p[4] << 8) | p[5];
            uint16_t rh = (p[6] << 8) | p[7];
            int32_t encoding = static_cast<int32_t>((p[8] << 24) | (p[9] << 16) | (p[10] << 8) | p[11]);

            if (encoding == -223) { // DesktopSize pseudo-encoding
                m_fbWidth = rw > 0 ? rw : 1080;
                m_fbHeight = rh > 0 ? rh : 1920;
                m_stride = m_fbWidth * 4;
                m_framebuffer.resize(m_stride * m_fbHeight, 0);
                LOG_QEMU_INFO("VncFrameReceiver: Display resolution switched to {}x{}", m_fbWidth, m_fbHeight);
                m_readBuffer.remove(0, 12);
                m_pendingRects--;
                emit frameReady(m_fbWidth, m_fbHeight, m_stride, m_framebuffer.data());
                continue;
            } else if (encoding == 0) { // Raw
                int pixelDataSize = rw * rh * 4;
                if (m_readBuffer.size() < 12 + pixelDataSize) {
                    return; // Wait for full frame tile data
                }
                const uint8_t* src = p + 12;
                for (int y = 0; y < rh; ++y) {
                    int dstRow = ry + y;
                    if (dstRow >= m_fbHeight) break;
                    int copyW = std::min<int>(rw, m_fbWidth - rx);
                    if (copyW <= 0) continue;
                    uint8_t* dst = m_framebuffer.data() + (dstRow * m_stride) + (rx * 4);
                    std::memcpy(dst, src + (y * rw * 4), copyW * 4);
                }
                m_readBuffer.remove(0, 12 + pixelDataSize);
                m_pendingRects--;
                if (m_pendingRects == 0) {
                    emit frameReady(m_fbWidth, m_fbHeight, m_stride, m_framebuffer.data());
                    requestFramebufferUpdate(true);
                }
                continue;
            } else {
                LOG_QEMU_WARN("VncFrameReceiver: Skipping unsupported encoding {}", encoding);
                m_readBuffer.remove(0, 12);
                m_pendingRects--;
                continue;
            }
        }

        uint8_t msgType = static_cast<uint8_t>(m_readBuffer[0]);
        if (msgType == 0) { // FramebufferUpdate
            if (m_readBuffer.size() < 4) return;
            uint16_t numRects = (static_cast<uint8_t>(m_readBuffer[2]) << 8) | static_cast<uint8_t>(m_readBuffer[3]);
            m_readBuffer.remove(0, 4);
            m_pendingRects = numRects;
        } else if (msgType == 1) { // SetColourMapEntries
            if (m_readBuffer.size() < 6) return;
            uint16_t numColors = (static_cast<uint8_t>(m_readBuffer[4]) << 8) | static_cast<uint8_t>(m_readBuffer[5]);
            int totalSize = 6 + (numColors * 6);
            if (m_readBuffer.size() < totalSize) return;
            m_readBuffer.remove(0, totalSize);
        } else if (msgType == 2) { // Bell
            m_readBuffer.remove(0, 1);
        } else if (msgType == 3) { // ServerCutText
            if (m_readBuffer.size() < 8) return;
            uint32_t textLen = (static_cast<uint8_t>(m_readBuffer[4]) << 24) |
                               (static_cast<uint8_t>(m_readBuffer[5]) << 16) |
                               (static_cast<uint8_t>(m_readBuffer[6]) << 8) |
                               static_cast<uint8_t>(m_readBuffer[7]);
            if (m_readBuffer.size() < 8 + static_cast<int>(textLen)) return;
            m_readBuffer.remove(0, 8 + textLen);
        } else {
            LOG_QEMU_WARN("VncFrameReceiver: Unknown message type {}, clearing buffer", msgType);
            m_readBuffer.clear();
            break;
        }
    }
}

void VncFrameReceiver::sendPointerEvent(int x, int y, uint8_t buttonMask) {
    if (m_state != VncState::Connected) return;

    x = std::clamp(x, 0, m_fbWidth - 1);
    y = std::clamp(y, 0, m_fbHeight - 1);

    uint8_t msg[6] = {0};
    msg[0] = 5; // PointerEvent message type
    msg[1] = buttonMask; // 1 = left click / touch down, 0 = touch up
    msg[2] = static_cast<uint8_t>((x >> 8) & 0xFF);
    msg[3] = static_cast<uint8_t>(x & 0xFF);
    msg[4] = static_cast<uint8_t>((y >> 8) & 0xFF);
    msg[5] = static_cast<uint8_t>(y & 0xFF);

    m_socket->write(reinterpret_cast<const char*>(msg), 6);
}

void VncFrameReceiver::sendKeyEvent(uint32_t keysym, bool down) {
    if (m_state != VncState::Connected) return;

    uint8_t msg[8] = {0};
    msg[0] = 4; // KeyEvent message type
    msg[1] = down ? 1 : 0;
    msg[2] = 0; msg[3] = 0; // padding
    msg[4] = static_cast<uint8_t>((keysym >> 24) & 0xFF);
    msg[5] = static_cast<uint8_t>((keysym >> 16) & 0xFF);
    msg[6] = static_cast<uint8_t>((keysym >> 8) & 0xFF);
    msg[7] = static_cast<uint8_t>(keysym & 0xFF);

    m_socket->write(reinterpret_cast<const char*>(msg), 8);
}

} // namespace qemu_backend
