#pragma once

#include <QWidget>
#include "rom_manager/rom_manager.h"

class RomCard : public QWidget {
    Q_OBJECT
public:
    explicit RomCard(const RomMetadata& meta, QWidget* parent = nullptr);
    ~RomCard() override = default;

signals:
    void uninstallRequested(const std::string& romName);
    void infoRequested(const RomMetadata& meta);

private:
    void setupUi();
    RomMetadata m_meta;
};
