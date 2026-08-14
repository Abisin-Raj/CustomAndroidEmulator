#pragma once

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTabWidget>
#include <memory>
#include "rom_manager/rom_manager.h"
#include "virtual_machine/vm_manager.h"
#include "boot_events/event_types.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;
    
    void autoLaunchRom(const QString& romName);

private slots:
    void onSidebarSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous);
    
    // ROM actions
    void onInstallRomClicked();
    void onLaunchClicked();
    void onRemoveClicked();
    void onRefreshClicked();
    void onRomInfoClicked();
    
    // Custom QEMU Path Browser
    void onBrowseQemuClicked();

    // VM State Slot Callbacks
    void onVmStateChanged(bool running);

    // Boot Pipeline Real-time Callbacks
    void onBootEventDispatched(const BootEvent& event);
    void onBootStateChanged(BootState state);

    // Helpers to update page content
    void refreshLogPanel();
    void updateDashboardStats();

private:
    void setupUi();
    void setupDashboardPage();
    void setupDisplayPage();
    void setupRomsPage();
    void setupSettingsPage();
    void setupLogsPage();
    void setupAboutPage();
    
    void loadRomsList();
    void resetBootChecklist();

    std::unique_ptr<RomManager> m_romManager;
    VmManager* m_vmManager = nullptr;
    class DisplayViewport* m_displayViewport = nullptr;
    class HardwareButtonWidget* m_hardwareButtonWidget = nullptr;
    
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
    
    // Page 3: Settings
    QLineEdit* m_qemuPathLineEdit = nullptr;
    QPushButton* m_browseQemuBtn = nullptr;

    // Page 4: Logs (Split layout)
    QListWidget* m_timelineListWidget = nullptr;
    QTextEdit* m_logsTextEdit = nullptr;
    QTextEdit* m_crashAnalyzerEdit = nullptr;
    QTabWidget* m_logTabWidget = nullptr;

    // Check states checklist labels
    QLabel* m_stateVmStarting = nullptr;
    QLabel* m_stateKernelRunning = nullptr;
    QLabel* m_stateUserspaceStarting = nullptr;
    QLabel* m_stateSystemServices = nullptr;
    QLabel* m_stateBootComplete = nullptr;
    QLabel* m_bootStatsLabel = nullptr;
    
    // Status Bar
    QLabel* m_statusLabel = nullptr;
};
