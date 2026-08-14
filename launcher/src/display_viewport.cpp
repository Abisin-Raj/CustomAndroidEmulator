#include "display_viewport.h"
#include "core/logger.h"
#include <QPaintEvent>

DisplayViewport::DisplayViewport(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(360, 640);
    setStyleSheet("background-color: #121212; border: 2px solid #2a2a2a; border-radius: 8px;");
}

void DisplayViewport::updateFramebuffer(int width, int height, int stride, const uint8_t* pixels) {
    if (!pixels || width <= 0 || height <= 0) return;

    QMutexLocker locker(&m_frameMutex);
    m_currentFrame = QImage(pixels, width, height, stride, QImage::Format_RGBA8888).copy();
    m_frameCount++;

    if (m_frameCount % 60 == 0) {
        LOG_INFO("DisplayViewport: Frame #{} rendered [{}x{} RGBA_8888]", m_frameCount, width, height);
    }

    update(); // Trigger repaint
}

void DisplayViewport::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QMutexLocker locker(&m_frameMutex);
    if (!m_currentFrame.isNull()) {
        // Center and scale image to fit widget aspect ratio
        QSize widgetSize = size();
        QImage scaled = m_currentFrame.scaled(widgetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        int x = (widgetSize.width() - scaled.width()) / 2;
        int y = (widgetSize.height() - scaled.height()) / 2;
        painter.drawImage(x, y, scaled);
    } else {
        // Draw placeholder offline screen
        painter.setPen(QColor("#888888"));
        painter.setFont(QFont("Segoe UI", 12));
        painter.drawText(rect(), Qt::AlignCenter, "Custom Android Emulator\n[Waiting for SurfaceFlinger Frame Commit...]");
    }
}
