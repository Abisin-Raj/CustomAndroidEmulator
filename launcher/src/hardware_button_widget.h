#pragma once
#include "hardware_keys.h"
#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>

/**
 * @brief Reusable widget displaying realistic physical hardware buttons for the mobile device.
 * Emits abstract HardwareKey signals with press and hold state tracking.
 */
class HardwareButtonWidget : public QWidget {
    Q_OBJECT
public:
    explicit HardwareButtonWidget(QWidget* parent = nullptr);

signals:
    void keyPressed(HardwareKey key);
    void keyReleased(HardwareKey key);

private:
    void setupUi();
    QPushButton* createButton(const QString& text, const QString& tooltip, const QString& colorClass);

    QPushButton* m_volUpBtn;
    QPushButton* m_volDownBtn;
    QPushButton* m_powerBtn;
};
