#pragma once

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <memory>
#include "rom_manager/rom_manager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onSidebarSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous);
    
    // ROM actions
    void onInstallRomClicked();
    void onLaunchClicked();
    void onRemoveClicked();
    void onRefreshClicked();
    void onRomInfoClicked();
    
    // Helpers to update page content
    void refreshLogPanel();
    void updateDashboardStats();

private:
    void setupUi();
    void setupDashboardPage();
    void setupRomsPage();
    void setupSettingsPage();
    void setupLogsPage();
    void setupAboutPage();
    
    void loadRomsList();

    std::unique_ptr<RomManager> m_romManager;
    
    // UI components
    QListWidget* m_sidebarList = nullptr;
    QStackedWidget* m_stackedWidget = nullptr;
    
    // Page 1: Dashboard
    QLabel* m_dashTotalRomsLabel = nullptr;
    QLabel* m_dashSummaryText = nullptr;
    
    // Page 2: ROMs
    QTableWidget* m_romsTable = nullptr;
    QPushButton* m_launchBtn = nullptr;
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_infoBtn = nullptr;
    
    // Page 4: Logs
    QTextEdit* m_logsTextEdit = nullptr;
    
    // Status Bar
    QLabel* m_statusLabel = nullptr;
};
