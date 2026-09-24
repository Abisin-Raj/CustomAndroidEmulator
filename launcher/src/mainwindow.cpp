#include "mainwindow.h"
#include "core/logger.h"
#include "core/config_manager.h"
#include "image_parser/image_parser.h"
#include "boot_manager/boot_manager.h"
#include "models/boot_report.h"
#include "boot_events/event_dispatcher.h"
#include "boot_pipeline/boot_pipeline_manager.h"
#include "display_viewport.h"
#include "hardware_button_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressDialog>
#include <QCoreApplication>
#include <QHeaderView>
#include <QTimer>
#include <QFormLayout>
#include <QStatusBar>
#include <QGroupBox>
#include <fstream>
#include <filesystem>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("Custom Android Emulator");
    resize(1200, 800);
    setMinimumSize(950, 650);

    // Initialize configuration files
    core::ConfigManager::initializeDefaultConfigs();

    QString romsDir = QCoreApplication::applicationDirPath() + "/roms";
    m_romManager = std::make_unique<RomManager>(romsDir.toStdString());
    m_vmManager = new VmManager(this);

    // Track active VM state changes
    connect(m_vmManager, &VmManager::vmStateChanged, this, &MainWindow::onVmStateChanged);

    // Connect real-time Boot Pipeline callbacks
    connect(m_vmManager->getDispatcher(), &EventDispatcher::eventDispatched, this, &MainWindow::onBootEventDispatched);
    connect(m_vmManager->getDispatcher(), &EventDispatcher::stateChanged, this, &MainWindow::onBootStateChanged);

    m_displayViewport = new DisplayViewport(this);
    connect(m_vmManager, &VmManager::frameReady, m_displayViewport, &DisplayViewport::updateFramebuffer);
    connect(m_displayViewport, &DisplayViewport::pointerEvent, m_vmManager, &VmManager::sendPointerEvent);
    connect(m_displayViewport, &DisplayViewport::keyEvent, m_vmManager, &VmManager::sendKeyEvent);

    setupUi();
    
    m_sidebarList->setCurrentRow(0);
    loadRomsList();
    updateDashboardStats();
}

void MainWindow::setupUi() {
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    m_sidebarList = new QListWidget(this);
    m_sidebarList->setFixedWidth(180);
    m_sidebarList->addItem("Dashboard");
    m_sidebarList->addItem("Display");
    m_sidebarList->addItem("ROMs");
    m_sidebarList->addItem("Settings");
    m_sidebarList->addItem("Logs");
    m_sidebarList->addItem("About");
    
    connect(m_sidebarList, &QListWidget::currentItemChanged, this, &MainWindow::onSidebarSelectionChanged);
    mainLayout->addWidget(m_sidebarList);

    m_stackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(m_stackedWidget);

    setupDashboardPage();
    setupDisplayPage();
    setupRomsPage();
    setupSettingsPage();
    setupLogsPage();
    setupAboutPage();

    m_statusLabel = new QLabel("Status: Ready", this);
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::onSidebarSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous) {
    if (!current) return;
    
    QString text = current->text();
    if (text == "Dashboard") {
        m_stackedWidget->setCurrentIndex(0);
        updateDashboardStats();
    } else if (text == "Display") {
        m_stackedWidget->setCurrentIndex(1);
    } else if (text == "ROMs") {
        m_stackedWidget->setCurrentIndex(2);
        loadRomsList();
    } else if (text == "Settings") {
        m_stackedWidget->setCurrentIndex(3);
    } else if (text == "Logs") {
        m_stackedWidget->setCurrentIndex(4);
        refreshLogPanel();
    } else if (text == "About") {
        m_stackedWidget->setCurrentIndex(5);
    }
}

void MainWindow::setupDisplayPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto* headerLayout = new QHBoxLayout();
    auto* titleLabel = new QLabel("Live Android Display Viewport & Hardware Controls", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_displayLaunchBtn = new QPushButton("▶ Launch Android VM", this);
    m_displayLaunchBtn->setStyleSheet("QPushButton { background-color: #238636; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; } QPushButton:hover { background-color: #2ea44f; }");
    connect(m_displayLaunchBtn, &QPushButton::clicked, this, &MainWindow::onLaunchClicked);
    headerLayout->addWidget(m_displayLaunchBtn);

    layout->addLayout(headerLayout);

    auto* displayContainer = new QHBoxLayout();
    displayContainer->addWidget(m_displayViewport, 1);

    m_hardwareButtonWidget = new HardwareButtonWidget(this);
    static int s_irqCounter = 0;
    auto handleKey = [this](HardwareKey key, bool pressed) {
        if (pressed) s_irqCounter++;
        QString keyName = (key == HardwareKey::VolumeUp) ? "VolumeUp" : (key == HardwareKey::VolumeDown) ? "VolumeDown" : "Power";
        int linuxCode = (key == HardwareKey::VolumeUp) ? 115 : (key == HardwareKey::VolumeDown) ? 114 : 116;
        m_hardwareButtonWidget->updateHardwareState(keyName, linuxCode, s_irqCounter, pressed ? "DOWN" : "UP");
        m_vmManager->sendHardwareKey(key, pressed);
    };

    connect(m_hardwareButtonWidget, &HardwareButtonWidget::keyPressed, this, [handleKey](HardwareKey key) {
        handleKey(key, true);
    });
    connect(m_hardwareButtonWidget, &HardwareButtonWidget::keyReleased, this, [handleKey](HardwareKey key) {
        handleKey(key, false);
    });
    connect(m_displayViewport, &DisplayViewport::hardwareKeyEvent, this, [handleKey](HardwareKey key, bool pressed) {
        handleKey(key, pressed);
    });

    displayContainer->addWidget(m_hardwareButtonWidget, 0, Qt::AlignVCenter);
    layout->addLayout(displayContainer, 1);

    m_stackedWidget->addWidget(page);
}

void MainWindow::setupDashboardPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    auto* titleLabel = new QLabel("Emulator Dashboard", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto* descLabel = new QLabel("Welcome to CustomAndroidEmulator launcher interface.", this);
    layout->addWidget(descLabel);

    auto* infoGroupBox = new QWidget(this);
    auto* gridLayout = new QGridLayout(infoGroupBox);
    gridLayout->setSpacing(10);

    gridLayout->addWidget(new QLabel("Total ROMs Installed:", this), 0, 0);
    m_dashTotalRomsLabel = new QLabel("0", this);
    m_dashTotalRomsLabel->setStyleSheet("font-weight: bold;");
    gridLayout->addWidget(m_dashTotalRomsLabel, 0, 1);

    gridLayout->addWidget(new QLabel("Storage Directory:", this), 1, 0);
    QString romsDir = QCoreApplication::applicationDirPath() + "/roms";
    auto* dirLabel = new QLabel(romsDir, this);
    gridLayout->addWidget(dirLabel, 1, 1);

    gridLayout->addWidget(new QLabel("Target Platform:", this), 2, 0);
    gridLayout->addWidget(new QLabel("Windows x86_64", this), 2, 1);

    gridLayout->addWidget(new QLabel("Virtualization Engine:", this), 3, 0);
    gridLayout->addWidget(new QLabel("QEMU Emulator Backend (Phase 5 Pipeline Active)", this), 3, 1);

    layout->addWidget(infoGroupBox);

    m_dashLaunchBtn = new QPushButton("▶ Launch Android ROM in Emulator Display (Fast Resume)", this);
    m_dashLaunchBtn->setStyleSheet("QPushButton { background-color: #238636; color: white; font-size: 14px; font-weight: bold; padding: 12px 24px; border-radius: 6px; } QPushButton:hover { background-color: #2ea44f; }");
    connect(m_dashLaunchBtn, &QPushButton::clicked, this, &MainWindow::onLaunchClicked);
    layout->addWidget(m_dashLaunchBtn);

    layout->addStretch();
    
    m_stackedWidget->addWidget(page);
}

void MainWindow::updateDashboardStats() {
    auto roms = m_romManager->getInstalledRoms();
    m_dashTotalRomsLabel->setText(QString::number(roms.size()));
}

void MainWindow::setupRomsPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

    auto* titleLabel = new QLabel("Installed ROM Packages", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    m_romsTable = new QTableWidget(this);
    m_romsTable->setColumnCount(5);
    m_romsTable->setHorizontalHeaderLabels({"ROM Name", "Target Device", "Codename", "Android Version", "Bootable Status"});
    m_romsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_romsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_romsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_romsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_romsTable);

    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(10);

    auto* installBtn = new QPushButton("Install ROM", this);
    connect(installBtn, &QPushButton::clicked, this, &MainWindow::onInstallRomClicked);
    actionLayout->addWidget(installBtn);

    m_launchBtn = new QPushButton("Launch", this);
    m_launchBtn->setEnabled(false);
    connect(m_launchBtn, &QPushButton::clicked, this, &MainWindow::onLaunchClicked);
    actionLayout->addWidget(m_launchBtn);

    m_removeBtn = new QPushButton("Remove", this);
    connect(m_removeBtn, &QPushButton::clicked, this, &MainWindow::onRemoveClicked);
    actionLayout->addWidget(m_removeBtn);

    m_infoBtn = new QPushButton("ROM Info", this);
    connect(m_infoBtn, &QPushButton::clicked, this, &MainWindow::onRomInfoClicked);
    actionLayout->addWidget(m_infoBtn);

    auto* refreshBtn = new QPushButton("Refresh", this);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    actionLayout->addWidget(refreshBtn);

    layout->addLayout(actionLayout);
    m_stackedWidget->addWidget(page);
}

void MainWindow::loadRomsList() {
    if (m_vmManager->isVmRunning()) return;

    m_romsTable->setRowCount(0);
    
    auto roms = m_romManager->getInstalledRoms();
    m_romsTable->setRowCount(static_cast<int>(roms.size()));

    for (size_t i = 0; i < roms.size(); ++i) {
        m_romsTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(roms[i].name)));
        m_romsTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(roms[i].device)));
        m_romsTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(roms[i].codename)));
        m_romsTable->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(roms[i].androidVersion)));
        m_romsTable->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(roms[i].status)));
    }
    
    bool hasRows = roms.size() > 0;
    m_removeBtn->setEnabled(hasRows);
    m_infoBtn->setEnabled(hasRows);
    m_launchBtn->setEnabled(hasRows);
    if (m_dashLaunchBtn) m_dashLaunchBtn->setEnabled(hasRows);
    if (m_displayLaunchBtn) m_displayLaunchBtn->setEnabled(hasRows);
    if (hasRows && m_romsTable->currentRow() < 0) {
        m_romsTable->selectRow(0);
    }
}

void MainWindow::onInstallRomClicked() {
    QString zipPath = QFileDialog::getOpenFileName(this, "Select Custom ROM ZIP", "", "Android ROM ZIPs (*.zip)");
    if (zipPath.isEmpty()) return;

    RomMetadata meta;
    std::string error;
    m_statusLabel->setText("Status: Validating zip...");
    QCoreApplication::processEvents();

    if (!m_romManager->validateRomZip(zipPath.toStdString(), meta, error)) {
        QMessageBox::critical(this, "Invalid ROM Package", 
            QString("The selected ZIP file is not a valid Android custom ROM.\n\nError: %1").arg(QString::fromStdString(error)));
        m_statusLabel->setText("Status: Validation failed");
        return;
    }

    QString details = QString(
        "A valid custom ROM package was detected!\n\n"
        "Name: %1\n"
        "Device: %2\n"
        "Android Version: %3\n"
        "Architecture: %4\n\n"
        "Would you like to install it now?\n"
        "(This will extract partition images into the emulator directory and run boot validation checks)"
    ).arg(QString::fromStdString(meta.name), QString::fromStdString(meta.device), QString::fromStdString(meta.androidVersion), QString::fromStdString(meta.architecture));

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm ROM Installation", details, QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        bool ok;
        QString defaultName = QString::fromStdString(meta.name);
        QString romName = QInputDialog::getText(
            this, "ROM Name", "Enter a name for this ROM installation:", 
            QLineEdit::Normal, defaultName, &ok
        );
        if (!ok || romName.trimmed().isEmpty()) {
            m_statusLabel->setText("Status: Installation cancelled");
            return;
        }

        m_statusLabel->setText("Status: Extracting & validating images...");
        QProgressDialog progress("Initializing ROM installation...", nullptr, 0, 100, this);
        progress.setWindowTitle("Installing ROM");
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(0);
        progress.setValue(0);
        progress.show();
        QCoreApplication::processEvents();

        auto progressCb = [&progress](const std::string& stage, float p) {
            int val = static_cast<int>(p * 100.0f);
            if (val > 100) val = 100;
            if (val < 0) val = 0;
            progress.setValue(val);
            progress.setLabelText(QString("%1\nProgress: %2%").arg(QString::fromStdString(stage)).arg(val));
            QCoreApplication::processEvents();
        };

        std::string installError;
        bool success = m_romManager->installRom(zipPath.toStdString(), romName.trimmed().toStdString(), installError, progressCb);
        progress.setValue(100);
        progress.close();

        if (success) {
            QMessageBox::information(this, "Installation Complete", QString("ROM '%1' has been successfully installed and verified!").arg(romName));
            m_statusLabel->setText("Status: Ready");
            loadRomsList();
        } else {
            QMessageBox::critical(this, "Installation Failed", QString("An error occurred during ROM installation:\n\n%1").arg(QString::fromStdString(installError)));
            m_statusLabel->setText("Status: Installation failed");
        }
    } else {
        m_statusLabel->setText("Status: Ready");
    }
}

void MainWindow::onLaunchClicked() {
    if (m_vmManager->isVmRunning()) {
        m_statusLabel->setText("Status: Stopping virtual machine...");
        m_vmManager->stopVm();
        return;
    }

    int row = m_romsTable->currentRow();
    if (row < 0 && m_romsTable->rowCount() > 0) {
        row = 0;
        m_romsTable->selectRow(0);
    }
    if (row < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a ROM from the table to launch.");
        return;
    }

    std::string name = m_romsTable->item(row, 0)->text().toStdString();
    QString romDir = QCoreApplication::applicationDirPath() + "/roms/" + QString::fromStdString(name);
    
    m_statusLabel->setText("Status: Booting guest virtual machine...");
    QCoreApplication::processEvents();

    m_timelineListWidget->clear();
    m_crashAnalyzerEdit->clear();
    m_logsTextEdit->clear();
    m_logTabWidget->setCurrentIndex(0); // View timeline
    resetBootChecklist();

    if (m_vmManager->startVm(name, romDir.toStdString())) {
        LOG_LAUNCHER_INFO("Virtual machine launcher spawned successfully.");
        // UI states are automatically updated in onVmStateChanged(true) via signal mapping
    } else {
        QMessageBox::critical(this, "Boot VM Failed", 
            "Failed to launch the target virtual machine.\nConfirm QEMU path is set in Settings and boot_config.json exists.");
        m_statusLabel->setText("Status: Launch Failed");
    }
}

void MainWindow::onVmStateChanged(bool running) {
    if (running) {
        m_statusLabel->setText("Status: Virtual Machine Active");
        m_launchBtn->setText("Stop VM");
        m_launchBtn->setEnabled(true);
        if (m_dashLaunchBtn) m_dashLaunchBtn->setText("⏹ Stop Android VM");
        if (m_displayLaunchBtn) {
            m_displayLaunchBtn->setText("⏹ Stop VM");
            m_displayLaunchBtn->setStyleSheet("QPushButton { background-color: #da3633; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; } QPushButton:hover { background-color: #f85149; }");
        }
        m_removeBtn->setEnabled(false);
        m_infoBtn->setEnabled(false);
        m_romsTable->setEnabled(false);
        m_sidebarList->setCurrentRow(1); // Auto-switch to live Display Viewport
    } else {
        m_statusLabel->setText("Status: Ready");
        m_launchBtn->setText("Launch");
        if (m_dashLaunchBtn) m_dashLaunchBtn->setText("▶ Launch Android ROM in Emulator Display (Fast Resume)");
        if (m_displayLaunchBtn) {
            m_displayLaunchBtn->setText("▶ Launch Android VM");
            m_displayLaunchBtn->setStyleSheet("QPushButton { background-color: #238636; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; } QPushButton:hover { background-color: #2ea44f; }");
        }
        m_romsTable->setEnabled(true);
        
        // Re-enable tabs
        for (int i = 0; i < m_sidebarList->count(); ++i) {
            m_sidebarList->item(i)->setFlags(m_sidebarList->item(i)->flags() | Qt::ItemIsEnabled);
        }

        // Show session stats overview
        QString statsSummary = QString::fromStdString(m_vmManager->getPipelineManager()->getSession().generateStats());
        m_bootStatsLabel->setText(statsSummary);

        loadRomsList();
    }
}

void MainWindow::onBootStateChanged(BootState state) {
    switch (state) {
        case BootState::VM_STARTING:
            m_stateVmStarting->setText("✓ VM Started");
            m_stateVmStarting->setStyleSheet("color: #2196F3; font-weight: bold;"); // Blue
            break;
        case BootState::KERNEL_LOADING:
            m_stateVmStarting->setText("✓ Kernel Loaded");
            m_stateVmStarting->setStyleSheet("color: #4CAF50; font-weight: bold;"); // Green
            break;
        case BootState::KERNEL_RUNNING:
            m_stateKernelRunning->setText("✓ Kernel Running");
            m_stateKernelRunning->setStyleSheet("color: #4CAF50; font-weight: bold;");
            break;
        case BootState::USERSPACE_STARTING:
            m_stateUserspaceStarting->setText("✓ Userspace Starting");
            m_stateUserspaceStarting->setStyleSheet("color: #4CAF50; font-weight: bold;");
            break;
        case BootState::SYSTEM_SERVICES:
            m_stateSystemServices->setText("✓ System Services");
            m_stateSystemServices->setStyleSheet("color: #4CAF50; font-weight: bold;");
            break;
        case BootState::BOOT_COMPLETE:
            m_stateBootComplete->setText("✓ Boot Completed");
            m_stateBootComplete->setStyleSheet("color: #4CAF50; font-weight: bold;");
            break;
        case BootState::EMULATION_FAILED:
            m_statusLabel->setText("Status: Emulation Failed / Panicked");
            break;
    }
}

void MainWindow::onBootEventDispatched(const BootEvent& event) {
    QString timeStr = QString("[%1s] ").arg(event.elapsedSeconds, 0, 'f', 2);
    QString text = timeStr + QString::fromStdString(event.details);
    
    // Add confidence annotation
    text += QString(" (Confidence: %1%)").arg(event.confidence * 100.0, 0, 'f', 0);
    
    auto* item = new QListWidgetItem(text, m_timelineListWidget);
    
    // Apply styling based on severity level
    switch (event.severity) {
        case EventSeverity::Info:
            item->setForeground(QBrush(QColor("#2196F3"))); // Blue
            break;
        case EventSeverity::Warning:
            item->setForeground(QBrush(QColor("#FF9800"))); // Orange
            break;
        case EventSeverity::Error:
        case EventSeverity::Critical:
            item->setForeground(QBrush(QColor("#F44336"))); // Red
            item->setFont(QFont("Segoe UI", -1, QFont::Bold));
            break;
    }

    m_timelineListWidget->addItem(item);
    m_timelineListWidget->scrollToBottom();

    // Trigger crash window automatically if critical panic event occurred
    if (event.type == BootEventType::KernelPanic) {
        m_logTabWidget->setCurrentIndex(2); // Focus Crash Tab

        std::string process = event.metadata.count("process") ? event.metadata.at("process") : "Unknown";
        std::string pid = event.metadata.count("pid") ? event.metadata.at("pid") : "Unknown";
        std::string cpu = event.metadata.count("cpu") ? event.metadata.at("cpu") : "Unknown";
        std::string cause = event.metadata.count("suggested_cause") ? event.metadata.at("suggested_cause") : "Unknown";
        std::string regs = event.metadata.count("registers") ? event.metadata.at("registers") : "";
        std::string trace = event.metadata.count("call_trace") ? event.metadata.at("call_trace") : "";

        QString crashReport = QString(
            "==================================================\n"
            "   GUEST SYSTEM KERNEL PANIC ANALYZER\n"
            "==================================================\n"
            "Critical panic detected during OS boot pipeline execution!\n\n"
            "● Reason:             %1\n"
            "● Process Comm:       %2 (PID: %3)\n"
            "● Faulting CPU:       Core %4\n"
            "● Suggested Cause:    %5\n\n"
            "--------------------------------------------------\n"
            "REGISTERS DUMP:\n"
            "--------------------------------------------------\n%6\n\n"
            "--------------------------------------------------\n"
            "CALL STACK BACKTRACE:\n"
            "--------------------------------------------------\n%7"
        ).arg(
            QString::fromStdString(event.details),
            QString::fromStdString(process),
            QString::fromStdString(pid),
            QString::fromStdString(cpu),
            QString::fromStdString(cause),
            QString::fromStdString(regs),
            QString::fromStdString(trace)
        );

        m_crashAnalyzerEdit->setPlainText(crashReport);

        // Mark the active check state as failed visually
        if (m_vmManager->getPipelineManager()->getContext().currentState < BootState::KERNEL_RUNNING) {
            m_stateKernelRunning->setText("✗ Kernel Panic");
            m_stateKernelRunning->setStyleSheet("color: #F44336; font-weight: bold;");
        } else if (m_vmManager->getPipelineManager()->getContext().currentState < BootState::USERSPACE_STARTING) {
            m_stateUserspaceStarting->setText("✗ Boot Failed");
            m_stateUserspaceStarting->setStyleSheet("color: #F44336; font-weight: bold;");
        } else {
            m_stateSystemServices->setText("✗ Systems Crashed");
            m_stateSystemServices->setStyleSheet("color: #F44336; font-weight: bold;");
        }
    }

    // Refresh quick stats counts
    QString statsSummary = QString::fromStdString(m_vmManager->getPipelineManager()->getSession().generateStats());
    m_bootStatsLabel->setText(statsSummary);
}

void MainWindow::onRemoveClicked() {
    int row = m_romsTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a ROM from the table to remove.");
        return;
    }

    std::string name = m_romsTable->item(row, 0)->text().toStdString();
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Uninstall ROM", 
        QString("Are you sure you want to uninstall '%1'?\nThis will permanently delete all extracted image files for this ROM.").arg(QString::fromStdString(name)),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        std::string error;
        m_statusLabel->setText("Status: Removing files...");
        if (m_romManager->uninstallRom(name, error)) {
            QMessageBox::information(this, "Uninstalled", QString("ROM '%1' has been uninstalled.").arg(QString::fromStdString(name)));
            m_statusLabel->setText("Status: Ready");
            loadRomsList();
        } else {
            QMessageBox::critical(this, "Error", QString("Failed to uninstall ROM:\n\n%1").arg(QString::fromStdString(error)));
            m_statusLabel->setText("Status: Error removing files");
        }
    }
}

void MainWindow::onRefreshClicked() {
    loadRomsList();
    m_statusLabel->setText("Status: Refreshed");
}

void MainWindow::onRomInfoClicked() {
    int row = m_romsTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a ROM from the table to inspect details.");
        return;
    }

    std::string name = m_romsTable->item(row, 0)->text().toStdString();
    auto roms = m_romManager->getInstalledRoms();
    
    auto it = std::find_if(roms.begin(), roms.end(), [&name](const RomMetadata& m) {
        return m.name == name;
    });

    if (it != roms.end()) {
        const auto& meta = *it;
        
        // Generate a fresh Boot validation report on-the-fly to show check results
        QString romDir = QCoreApplication::applicationDirPath() + "/roms/" + QString::fromStdString(meta.name);
        auto report = BootManager::validateAndPrepareBoot(meta.name, romDir.toStdString());

        QString warningsStr = "None";
        if (!report.warnings.empty()) {
            warningsStr = "";
            for (const auto& w : report.warnings) {
                warningsStr += "  ⚠️ " + QString::fromStdString(w) + "\n";
            }
        }

        QString errorsStr = "None";
        if (!report.errors.empty()) {
            errorsStr = "";
            for (const auto& e : report.errors) {
                errorsStr += "  ❌ " + QString::fromStdString(e) + "\n";
            }
        }

        QString recsStr = "None";
        if (!report.recommendations.empty()) {
            recsStr = "";
            for (const auto& r : report.recommendations) {
                recsStr += "  💡 " + QString::fromStdString(r) + "\n";
            }
        }

        QString imagesListStr = "None";
        if (!meta.images.empty()) {
            imagesListStr = "";
            for (const auto& img : meta.images) {
                imagesListStr += "  - " + QString::fromStdString(img) + "\n";
            }
        }

        QString info = QString(
            "Detailed ROM Information (Parsed & Verified):\n\n"
            "ROM Name: %1\n"
            "Device Model: %2\n"
            "Codename / Board: %3\n"
            "OS / Android version: %4\n"
            "Architecture: %5\n"
            "Kernel Release: %6\n"
            "Partition Layout: %7\n"
            "Verified Boot (AVB): %8\n\n"
            "--------------------------------------------------\n"
            "Boot validation checklist:\n"
            "Status: %9\n"
            "Virtual Machine Profile: %10\n\n"
            "Errors:\n%11\n"
            "Warnings:\n%12\n"
            "Recommendations:\n%13\n"
            "--------------------------------------------------\n"
            "Extracted partition image files:\n%14"
        ).arg(
            QString::fromStdString(meta.name),
            QString::fromStdString(meta.device),
            QString::fromStdString(meta.codename),
            QString::fromStdString(meta.androidVersion),
            QString::fromStdString(meta.architecture),
            QString::fromStdString(meta.kernel),
            QString::fromStdString(meta.partitionLayout),
            QString::fromStdString(meta.avbStatus),
            QString::fromStdString(report.status),
            QString::fromStdString(report.vmProfile),
            errorsStr,
            warningsStr,
            recsStr,
            imagesListStr
        );

        QMessageBox::information(this, "Detailed ROM Verification Report", info);
    }
}

void MainWindow::setupSettingsPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    auto* titleLabel = new QLabel("System Settings", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    // QEMU binary path picker
    auto* qemuLayout = new QHBoxLayout();
    m_qemuPathLineEdit = new QLineEdit(this);
    m_qemuPathLineEdit->setText(QString::fromStdString(core::ConfigManager::getQemuPath()));
    m_qemuPathLineEdit->setPlaceholderText("Detecting in system PATH / standard folders...");
    
    // Auto save on text changed
    connect(m_qemuPathLineEdit, &QLineEdit::textChanged, this, [](const QString& text){
        core::ConfigManager::setQemuPath(text.toStdString());
    });

    m_browseQemuBtn = new QPushButton("Browse...", this);
    connect(m_browseQemuBtn, &QPushButton::clicked, this, &MainWindow::onBrowseQemuClicked);
    qemuLayout->addWidget(m_qemuPathLineEdit);
    qemuLayout->addWidget(m_browseQemuBtn);

    formLayout->addRow("Custom QEMU Path:", qemuLayout);

    QString romsDir = QCoreApplication::applicationDirPath() + "/roms";
    formLayout->addRow("ROMs Storage Path:", new QLabel(romsDir, this));
    formLayout->addRow("Default RAM Allocation:", new QLabel("1024 MB (Dynamic profile managed)", this));
    formLayout->addRow("Virtual CPU Cores:", new QLabel("Virtual Board CPU (dynamic match)", this));
    formLayout->addRow("Emulation Target:", new QLabel("ARM64 (aarch64)", this));
    formLayout->addRow("Display Mode:", new QLabel("SDL Display (GPU Accelerated)", this));

    layout->addLayout(formLayout);
    layout->addStretch();
    
    m_stackedWidget->addWidget(page);
}

void MainWindow::onBrowseQemuClicked() {
    QString qemuFile = QFileDialog::getOpenFileName(this, "Select QEMU Executable", "", "QEMU Executable (*.exe)");
    if (!qemuFile.isEmpty()) {
        m_qemuPathLineEdit->setText(qemuFile);
        core::ConfigManager::setQemuPath(qemuFile.toStdString());
        LOG_LAUNCHER_INFO("Selected custom QEMU path: {}", qemuFile.toStdString());
    }
}

void MainWindow::setupLogsPage() {
    auto* page = new QWidget(this);
    auto* mainHLayout = new QHBoxLayout(page);
    mainHLayout->setContentsMargins(10, 10, 10, 10);
    mainHLayout->setSpacing(15);

    // Left Panel: Checkpoints & Stats
    auto* leftWidget = new QWidget(this);
    leftWidget->setFixedWidth(210);
    auto* leftVLayout = new QVBoxLayout(leftWidget);
    leftVLayout->setContentsMargins(0, 0, 0, 0);
    leftVLayout->setSpacing(10);

    auto* checkGroup = new QGroupBox("Boot Checkpoints", this);
    auto* checkLayout = new QVBoxLayout(checkGroup);
    checkLayout->setSpacing(8);

    m_stateVmStarting = new QLabel("○ Preparing", this);
    m_stateKernelRunning = new QLabel("○ Kernel Running", this);
    m_stateUserspaceStarting = new QLabel("○ Userspace Starting", this);
    m_stateSystemServices = new QLabel("○ System Services", this);
    m_stateBootComplete = new QLabel("○ Boot Completed", this);

    checkLayout->addWidget(m_stateVmStarting);
    checkLayout->addWidget(m_stateKernelRunning);
    checkLayout->addWidget(m_stateUserspaceStarting);
    checkLayout->addWidget(m_stateSystemServices);
    checkLayout->addWidget(m_stateBootComplete);
    checkLayout->addStretch();

    leftVLayout->addWidget(checkGroup);

    auto* statsGroup = new QGroupBox("Session Statistics", this);
    auto* statsLayout = new QVBoxLayout(statsGroup);
    m_bootStatsLabel = new QLabel("No active boot session.", this);
    m_bootStatsLabel->setWordWrap(true);
    statsLayout->addWidget(m_bootStatsLabel);
    
    leftVLayout->addWidget(statsGroup);
    leftVLayout->addStretch();

    mainHLayout->addWidget(leftWidget);

    // Right Panel: Tab Widgets
    m_logTabWidget = new QTabWidget(this);

    m_timelineListWidget = new QListWidget(this);
    m_logTabWidget->addTab(m_timelineListWidget, "Boot Timeline");

    m_logsTextEdit = new QTextEdit(this);
    m_logsTextEdit->setReadOnly(true);
    QFont monoFont("Courier New");
    monoFont.setStyleHint(QFont::Monospace);
    m_logsTextEdit->setFont(monoFont);
    
    m_logTabWidget->addTab(m_logsTextEdit, "Console Output");

    m_crashAnalyzerEdit = new QTextEdit(this);
    m_crashAnalyzerEdit->setReadOnly(true);
    m_crashAnalyzerEdit->setFont(monoFont);
    m_logTabWidget->addTab(m_crashAnalyzerEdit, "Crash Analyzer");

    mainHLayout->addWidget(m_logTabWidget);

    // Default checklist labels stylesheet
    resetBootChecklist();

    m_stackedWidget->addWidget(page);
}

void MainWindow::resetBootChecklist() {
    m_stateVmStarting->setText("○ Preparing");
    m_stateKernelRunning->setText("○ Kernel Running");
    m_stateUserspaceStarting->setText("○ Userspace Starting");
    m_stateSystemServices->setText("○ System Services");
    m_stateBootComplete->setText("○ Boot Completed");

    QString style = "color: #757575; font-size: 11pt;"; // Gray color
    m_stateVmStarting->setStyleSheet(style);
    m_stateKernelRunning->setStyleSheet(style);
    m_stateUserspaceStarting->setStyleSheet(style);
    m_stateSystemServices->setStyleSheet(style);
    m_stateBootComplete->setStyleSheet(style);
}

void MainWindow::refreshLogPanel() {
    QString logPath = QCoreApplication::applicationDirPath() + "/logs/launcher.log";
    std::ifstream in(logPath.toStdString());
    if (in) {
        std::stringstream ss;
        ss << in.rdbuf();
        m_logsTextEdit->setPlainText(QString::fromStdString(ss.str()));
    } else {
        m_logsTextEdit->setPlainText("[System Logging Output]\nLogs are printed to stdout console.\nInitialize simulator to view tracing details.");
    }
}

void MainWindow::setupAboutPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    auto* titleLabel = new QLabel("About CustomAndroidEmulator", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto* aboutText = new QLabel(
        "CustomAndroidEmulator Launcher\n"
        "Version 0.5.0 (Phase 5: Android Boot Pipeline)\n\n"
        "An ambitious systems-programming project aiming to run custom Android ROM packages "
        "inside a dynamic virtualization environment on Windows using a modular QEMU backend.\n\n"
        "Phase 5 Features:\n"
        " - Decoupled IBootDetector structures (Kernel, Init, Panic)\n"
        " - EventDispatcher broadcasts signal notifications across components\n"
        " - Register/CPU cores/process PID tracing inside PanicDetector\n"
        " - Dynamic checklist checklist mapping generic boot checkpoints\n"
        " - Session timings logs saved under logs/boot_<ROM>_<timestamp>.log\n\n"
        "Developed by abisin-raj <abisinr201@gmail.com>.",
        this
    );
    aboutText->setWordWrap(true);
    layout->addWidget(aboutText);

    layout->addStretch();
    m_stackedWidget->addWidget(page);
}

void MainWindow::autoLaunchRom(const QString& romName) {
    QString romDir = QCoreApplication::applicationDirPath() + "/roms/" + romName;
    m_timelineListWidget->clear();
    m_crashAnalyzerEdit->clear();
    m_logsTextEdit->clear();
    m_logTabWidget->setCurrentIndex(0);
    resetBootChecklist();
    
    LOG_LAUNCHER_INFO("Auto-launching ROM: {}", romName.toStdString());
    if (m_vmManager->startVm(romName.toStdString(), romDir.toStdString())) {
        LOG_LAUNCHER_INFO("Virtual machine launcher spawned successfully via auto-launch.");
    } else {
        LOG_LAUNCHER_ERROR("Failed to auto-launch ROM: {}", romName.toStdString());
    }
}
