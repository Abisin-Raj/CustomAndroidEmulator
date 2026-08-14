#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QTimer>
#include <vector>
#include <cstdint>

namespace qemu_backend {

enum class VncState {
    Disconnected,
    Connecting,
    WaitingProtocolVersion,
    WaitingSecurityTypes,
    WaitingSecurityResult,
    WaitingServerInit,
    Connected
};

class VncFrameReceiver : public QObject {
    Q_OBJECT
public:
    explicit VncFrameReceiver(QObject* parent = nullptr);
    ~VncFrameReceiver() override;

    /**
     * @brief Connects to the QEMU VNC server on the specified host and port.
     */
    void connectToServer(const QString& host = "127.0.0.1", quint16 port = 5900);

    /**
     * @brief Disconnects from the VNC server.
     */
    void disconnectFromServer();

    /**
     * @brief Checks if actively connected and receiving frames.
     */
    bool isConnected() const;

    /**
     * @brief Dispatches pointer (touch/mouse) events to the guest.
     */
    void sendPointerEvent(int x, int y, uint8_t buttonMask);

    /**
     * @brief Dispatches key events to the guest.
     */
    void sendKeyEvent(uint32_t keysym, bool down);

signals:
    void frameReady(int width, int height, int stride, const uint8_t* pixels);
    void connectionStateChanged(bool connected);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void requestFramebufferUpdate(bool incremental = true);

private:
    void processProtocolVersion();
    void processSecurityTypes();
    void processSecurityResult();
    void processServerInit();
    void processServerMessages();

    void sendPixelFormat();
    void sendEncodings();

    QTcpSocket* m_socket = nullptr;
    VncState m_state = VncState::Disconnected;

    QByteArray m_readBuffer;
    int m_fbWidth = 1080;
    int m_fbHeight = 1920;
    int m_stride = 4320;
    std::vector<uint8_t> m_framebuffer;

    int m_pendingRects = 0;
    bool m_updatePending = false;
    QTimer* m_retryTimer = nullptr;
    QTimer* m_pollTimer = nullptr;
    QString m_host = "127.0.0.1";
    quint16 m_port = 5900;
};

} // namespace qemu_backend
