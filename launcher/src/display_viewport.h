#pragma once

#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QMutex>
#include <cstdint>

class DisplayViewport : public QWidget {
    Q_OBJECT
public:
    explicit DisplayViewport(QWidget* parent = nullptr);
    ~DisplayViewport() override = default;

    /**
     * @brief Updates the viewport with a new raw RGBA framebuffer commit.
     */
    void updateFramebuffer(int width, int height, int stride, const uint8_t* pixels);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_currentFrame;
    QMutex m_frameMutex;
    int m_frameCount = 0;
};
