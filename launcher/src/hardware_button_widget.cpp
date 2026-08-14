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

    layout->addSpacing(20);

    // Hardware Input Inspector Box
    QFrame* inspectorFrame = new QFrame(this);
    inspectorFrame->setStyleSheet("QFrame { background: #161b22; border: 1px solid #30363d; border-radius: 8px; padding: 6px; }");
    QVBoxLayout* inspLayout = new QVBoxLayout(inspectorFrame);
    inspLayout->setContentsMargins(6, 6, 6, 6);
    inspLayout->setSpacing(4);

    QLabel* inspTitle = new QLabel("INSPECTOR", inspectorFrame);
    inspTitle->setAlignment(Qt::AlignCenter);
    inspTitle->setStyleSheet("color: #58a6ff; font-size: 9px; font-weight: bold; letter-spacing: 1px; border: none;");
    inspLayout->addWidget(inspTitle);

    m_lastKeyLabel = new QLabel("Key: None", inspectorFrame);
    m_lastKeyLabel->setStyleSheet("color: #c9d1d9; font-size: 10px; border: none;");
    inspLayout->addWidget(m_lastKeyLabel);

    m_codeLabel = new QLabel("Code: -", inspectorFrame);
    m_codeLabel->setStyleSheet("color: #8b949e; font-size: 10px; border: none;");
    inspLayout->addWidget(m_codeLabel);

    m_irqLabel = new QLabel("IRQs: 0", inspectorFrame);
    m_irqLabel->setStyleSheet("color: #3fb950; font-size: 10px; font-weight: bold; border: none;");
    inspLayout->addWidget(m_irqLabel);

    m_driverStatusLabel = new QLabel("Status: Active", inspectorFrame);
    m_driverStatusLabel->setStyleSheet("color: #8b949e; font-size: 9px; border: none;");
    inspLayout->addWidget(m_driverStatusLabel);

    layout->addWidget(inspectorFrame);

    layout->addStretch();
    setStyleSheet("background: transparent;");
}

void HardwareButtonWidget::updateHardwareState(const QString& lastKey, int linuxCode, int irqCount, const QString& driverStatus) {
    if (m_lastKeyLabel) m_lastKeyLabel->setText(QString("Key: %1").arg(lastKey));
    if (m_codeLabel) m_codeLabel->setText(QString("Code: %1").arg(linuxCode));
    if (m_irqLabel) m_irqLabel->setText(QString("IRQs: %1").arg(irqCount));
    if (m_driverStatusLabel) m_driverStatusLabel->setText(QString("Status: %1").arg(driverStatus));
}
