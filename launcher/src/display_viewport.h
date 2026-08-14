#pragma once

#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QMutex>
#include <cstdint>

#include "hardware_keys.h"

class DisplayViewport : public QWidget {
    Q_OBJECT
public:
    explicit DisplayViewport(QWidget* parent = nullptr);
    ~DisplayViewport() override = default;

    /**
     * @brief Updates the viewport with a new raw RGBA framebuffer commit.
     */
    void updateFramebuffer(int width, int height, int stride, const uint8_t* pixels);

signals:
    void pointerEvent(int x, int y, uint8_t buttonMask);
    void keyEvent(uint32_t keysym, bool down);
    void hardwareKeyEvent(HardwareKey key, bool pressed);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    QPoint mapToFramebuffer(const QPoint& pos) const;

    QImage m_currentFrame;
    QMutex m_frameMutex;
    int m_frameCount = 0;
    int m_fbWidth = 1080;
    int m_fbHeight = 1920;
};
