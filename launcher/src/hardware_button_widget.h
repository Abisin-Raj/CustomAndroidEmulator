#pragma once
#include "hardware_keys.h"
#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

/**
 * @brief Reusable widget displaying realistic physical hardware buttons for the mobile device
 * and a real-time Hardware Input Inspector panel.
 */
class HardwareButtonWidget : public QWidget {
    Q_OBJECT
public:
    explicit HardwareButtonWidget(QWidget* parent = nullptr);

    /**
     * @brief Updates the real-time Hardware Input Inspector status.
     */
    void updateHardwareState(const QString& lastKey, int linuxCode, int irqCount, const QString& driverStatus);

signals:
    void keyPressed(HardwareKey key);
    void keyReleased(HardwareKey key);

private:
    void setupUi();
    QPushButton* createButton(const QString& text, const QString& tooltip, const QString& colorClass);

    QPushButton* m_volUpBtn;
    QPushButton* m_volDownBtn;
    QPushButton* m_powerBtn;

    QLabel* m_lastKeyLabel;
    QLabel* m_codeLabel;
    QLabel* m_irqLabel;
    QLabel* m_driverStatusLabel;
};
