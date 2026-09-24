#include "display_viewport.h"
#include "core/logger.h"
#include <QPaintEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <algorithm>

static uint32_t qtKeyToKeysym(int key) {
    switch (key) {
        case Qt::Key_Up:         return 0xff52; // XK_Up -> KEY_VOLUMEUP
        case Qt::Key_Down:       return 0xff54; // XK_Down -> KEY_VOLUMEDOWN
        case Qt::Key_Return:
        case Qt::Key_Enter:      return 0xff0d; // XK_Return -> KEY_POWER / KEY_ENTER
        case Qt::Key_Escape:
        case Qt::Key_Back:       return 0xff1b; // XK_Escape -> KEY_BACK
        case Qt::Key_VolumeUp:   return 0x1008ff13;
        case Qt::Key_VolumeDown: return 0x1008ff11;
        case Qt::Key_PowerOff:   return 0x1008ff2a;
        case Qt::Key_Home:       return 0xff50;
        case Qt::Key_Menu:       return 0xff67;
        default:
            if (key >= 0x20 && key <= 0x7e) {
                return static_cast<uint32_t>(key);
            }
            return 0;
    }
}

DisplayViewport::DisplayViewport(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(270, 480);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet("background-color: #121212; border: 2px solid #2a2a2a; border-radius: 8px;");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void DisplayViewport::updateFramebuffer(int width, int height, int stride, const uint8_t* pixels) {
    if (!pixels || width <= 0 || height <= 0) return;

    QMutexLocker locker(&m_frameMutex);
    m_fbWidth = width;
    m_fbHeight = height;
    m_currentFrame = QImage(pixels, width, height, stride, QImage::Format_RGBX8888).copy();
    m_frameCount++;

    if (m_frameCount % 60 == 0 || m_frameCount == 1) {
        LOG_LAUNCHER_INFO("DisplayViewport: Frame #{} rendered [{}x{} RGBX_8888]", m_frameCount, width, height);
    }

    update(); // Trigger repaint
}

QPoint DisplayViewport::mapToFramebuffer(const QPoint& pos) const {
    if (m_fbWidth <= 0 || m_fbHeight <= 0) return QPoint(0, 0);

    QSize widgetSize = size();
    QSize scaledSize = QSize(m_fbWidth, m_fbHeight).scaled(widgetSize - QSize(20, 20), Qt::KeepAspectRatio);
    
    int offsetX = (widgetSize.width() - scaledSize.width()) / 2;
    int offsetY = (widgetSize.height() - scaledSize.height()) / 2;

    int relX = pos.x() - offsetX;
    int relY = pos.y() - offsetY;

    if (scaledSize.width() <= 0 || scaledSize.height() <= 0) return QPoint(0, 0);

    int fbX = (relX * m_fbWidth) / scaledSize.width();
    int fbY = (relY * m_fbHeight) / scaledSize.height();

    fbX = std::clamp(fbX, 0, m_fbWidth - 1);
    fbY = std::clamp(fbY, 0, m_fbHeight - 1);

    return QPoint(fbX, fbY);
}

void DisplayViewport::mousePressEvent(QMouseEvent* event) {
    setFocus();
    if (event->button() == Qt::LeftButton) {
        QPoint p = mapToFramebuffer(event->pos());
        emit pointerEvent(p.x(), p.y(), 1);
    }
}

void DisplayViewport::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        QPoint p = mapToFramebuffer(event->pos());
        emit pointerEvent(p.x(), p.y(), 1);
    }
}

void DisplayViewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QPoint p = mapToFramebuffer(event->pos());
        emit pointerEvent(p.x(), p.y(), 0);
    }
}

void DisplayViewport::keyPressEvent(QKeyEvent* event) {
    int k = event->key();
    if (k == Qt::Key_Up || k == Qt::Key_W) {
        emit hardwareKeyEvent(HardwareKey::VolumeUp, true);
        event->accept();
        return;
    } else if (k == Qt::Key_Down || k == Qt::Key_S) {
        emit hardwareKeyEvent(HardwareKey::VolumeDown, true);
        event->accept();
        return;
    } else if (k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Space || k == Qt::Key_P) {
        emit hardwareKeyEvent(HardwareKey::Power, true);
        event->accept();
        return;
    }

    uint32_t keysym = qtKeyToKeysym(k);
    if (keysym != 0) {
        LOG_LAUNCHER_INFO("DisplayViewport: Key press -> keysym 0x{:04x}", keysym);
        emit keyEvent(keysym, true);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void DisplayViewport::keyReleaseEvent(QKeyEvent* event) {
    int k = event->key();
    if (k == Qt::Key_Up || k == Qt::Key_W) {
        emit hardwareKeyEvent(HardwareKey::VolumeUp, false);
        event->accept();
        return;
    } else if (k == Qt::Key_Down || k == Qt::Key_S) {
        emit hardwareKeyEvent(HardwareKey::VolumeDown, false);
        event->accept();
        return;
    } else if (k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Space || k == Qt::Key_P) {
        emit hardwareKeyEvent(HardwareKey::Power, false);
        event->accept();
        return;
    }

    uint32_t keysym = qtKeyToKeysym(k);
    if (keysym != 0) {
        LOG_LAUNCHER_INFO("DisplayViewport: Key release -> keysym 0x{:04x}", keysym);
        emit keyEvent(keysym, false);
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void DisplayViewport::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Background
    painter.fillRect(rect(), QColor("#0d1117"));

    QMutexLocker locker(&m_frameMutex);
    QSize widgetSize = size();

    if (!m_currentFrame.isNull()) {
        QSize scaled = m_currentFrame.size().scaled(widgetSize - QSize(20, 20), Qt::KeepAspectRatio);
        int x = (widgetSize.width() - scaled.width()) / 2;
        int y = (widgetSize.height() - scaled.height()) / 2;

        // Draw phone bezel
        QRect phoneRect(x - 4, y - 4, scaled.width() + 8, scaled.height() + 8);
        painter.setPen(QPen(QColor("#30363d"), 2));
        painter.setBrush(QColor("#161b22"));
        painter.drawRoundedRect(phoneRect, 6, 6);

        // Draw active framebuffer
        painter.drawImage(QRect(x, y, scaled.width(), scaled.height()), m_currentFrame);
    } else {
        // Draw placeholder offline screen
        painter.setPen(QColor("#8b949e"));
        painter.setFont(QFont("Segoe UI", 12, QFont::Bold));
        painter.drawText(rect(), Qt::AlignCenter, "Custom Android Emulator\n[Waiting for QEMU Display Scanout...]");
    }
}
