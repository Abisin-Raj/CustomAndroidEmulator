#include "hardware_button_widget.h"
#include <QLabel>
#include <QGraphicsDropShadowEffect>

HardwareButtonWidget::HardwareButtonWidget(QWidget* parent)
    : QWidget(parent)
    , m_volUpBtn(nullptr)
    , m_volDownBtn(nullptr)
    , m_powerBtn(nullptr) {
    setupUi();
}

QPushButton* HardwareButtonWidget::createButton(const QString& text, const QString& tooltip, const QString& colorClass) {
    QPushButton* btn = new QPushButton(text, this);
    btn->setToolTip(tooltip);
    btn->setFixedSize(48, 64);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus); // Do not steal focus from viewport keyboard events

    // Premium styling with press animations
    btn->setStyleSheet(QString(
        "QPushButton {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2c3240, stop:1 #1c202a);"
        "   color: #e0e6ed;"
        "   font-size: 16px;"
        "   font-weight: bold;"
        "   border: 1px solid #3d4556;"
        "   border-radius: 8px;"
        "   padding: 4px;"
        "}"
        "QPushButton:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3a4254, stop:1 #242936);"
        "   border: 1px solid %1;"
        "}"
        "QPushButton:pressed {"
        "   background: %1;"
        "   color: #ffffff;"
        "   border: 1px solid #ffffff;"
        "   padding-top: 6px;"
        "}"
    ).arg(colorClass));

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(btn);
    shadow->setBlurRadius(8);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 2);
    btn->setGraphicsEffect(shadow);

    return btn;
}

void HardwareButtonWidget::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 8, 4, 8);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    QLabel* title = new QLabel("KEYS", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color: #7b889b; font-size: 10px; font-weight: bold; letter-spacing: 1px;");
    layout->addWidget(title);

    // 1. Volume Up
    m_volUpBtn = createButton("+", "Volume Up (↑ / W)", "#388bfd");
    layout->addWidget(m_volUpBtn);
    connect(m_volUpBtn, &QPushButton::pressed, this, [this]() {
        emit keyPressed(HardwareKey::VolumeUp);
    });
    connect(m_volUpBtn, &QPushButton::released, this, [this]() {
        emit keyReleased(HardwareKey::VolumeUp);
    });

    // 2. Volume Down
    m_volDownBtn = createButton("-", "Volume Down (↓ / S)", "#388bfd");
    layout->addWidget(m_volDownBtn);
    connect(m_volDownBtn, &QPushButton::pressed, this, [this]() {
        emit keyPressed(HardwareKey::VolumeDown);
    });
    connect(m_volDownBtn, &QPushButton::released, this, [this]() {
        emit keyReleased(HardwareKey::VolumeDown);
    });

    layout->addSpacing(16);

    // 3. Power Button
    m_powerBtn = createButton("⏻", "Power / Select (Enter / Space)", "#f85149");
    layout->addWidget(m_powerBtn);
    connect(m_powerBtn, &QPushButton::pressed, this, [this]() {
        emit keyPressed(HardwareKey::Power);
    });
    connect(m_powerBtn, &QPushButton::released, this, [this]() {
        emit keyReleased(HardwareKey::Power);
    });

    layout->addStretch();
    setStyleSheet("background: transparent;");
}
