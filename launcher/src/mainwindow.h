#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QScrollArea>
#include <QGridLayout>
#include <memory>
#include "rom_manager/rom_manager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onInstallRomClicked();
    void onSettingsClicked();
    void onUninstallRequested(const std::string& romName);
    void onInfoRequested(const RomMetadata& meta);

private:
    void setupUi();
    void loadInstalledRoms();
    void applyTheme();

    std::unique_ptr<RomManager> m_romManager;

    QWidget* m_cardsContainer = nullptr;
    QGridLayout* m_cardsLayout = nullptr;
    QLabel* m_noRomsLabel = nullptr;
    QScrollArea* m_scrollArea = nullptr;
};
