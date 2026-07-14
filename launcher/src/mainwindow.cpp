#include "mainwindow.h"
#include "core/logger.h"
#include "image_parser/image_parser.h"
#include "boot_manager/boot_manager.h"
#include "models/boot_report.h"
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
#include <fstream>
#include <filesystem>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("Custom Android Emulator");
    resize(850, 520);
    setMinimumSize(700, 400);

    QString romsDir = QCoreApplication::applicationDirPath() + "/roms";
    m_romManager = std::make_unique<RomManager>(romsDir.toStdString());

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
    m_sidebarList->addItem("ROMs");
    m_sidebarList->addItem("Settings");
    m_sidebarList->addItem("Logs");
    m_sidebarList->addItem("About");
    
    connect(m_sidebarList, &QListWidget::currentItemChanged, this, &MainWindow::onSidebarSelectionChanged);
    mainLayout->addWidget(m_sidebarList);

    m_stackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(m_stackedWidget);

    setupDashboardPage();
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
    } else if (text == "ROMs") {
        m_stackedWidget->setCurrentIndex(1);
        loadRomsList();
    } else if (text == "Settings") {
        m_stackedWidget->setCurrentIndex(2);
    } else if (text == "Logs") {
        m_stackedWidget->setCurrentIndex(3);
        refreshLogPanel();
    } else if (text == "About") {
        m_stackedWidget->setCurrentIndex(4);
    }
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
    gridLayout->addWidget(new QLabel("QEMU Emulator Backend (Pending Phase 4)", this), 3, 1);

    layout->addWidget(infoGroupBox);
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
    m_launchBtn->setToolTip("Virtualization engine integration will occur in Phase 4");
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
        QProgressDialog progress("Extracting images & validating boot parameters, please wait...", nullptr, 0, 0, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(0);
        progress.show();
        QCoreApplication::processEvents();

        std::string installError;
        bool success = m_romManager->installRom(zipPath.toStdString(), romName.trimmed().toStdString(), installError);
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
    // Placeholder
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
            "Boot validation checklist (Phase 3):\n"
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

    QString romsDir = QCoreApplication::applicationDirPath() + "/roms";
    formLayout->addRow("ROMs Storage Path:", new QLabel(romsDir, this));
    formLayout->addRow("Default RAM Allocation:", new QLabel("4096 MB", this));
    formLayout->addRow("Virtual CPU Cores:", new QLabel("4 Cores", this));
    formLayout->addRow("Emulation Target:", new QLabel("ARM64 (aarch64)", this));
    formLayout->addRow("Display Mode:", new QLabel("SDL Display (GPU Accelerated)", this));

    layout->addLayout(formLayout);
    layout->addStretch();
    
    m_stackedWidget->addWidget(page);
}

void MainWindow::setupLogsPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

    auto* titleLabel = new QLabel("System Execution Logs", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    m_logsTextEdit = new QTextEdit(this);
    m_logsTextEdit->setReadOnly(true);
    QFont monoFont("Courier New");
    monoFont.setStyleHint(QFont::Monospace);
    m_logsTextEdit->setFont(monoFont);
    layout->addWidget(m_logsTextEdit);

    auto* refreshBtn = new QPushButton("Refresh Logs", this);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshLogPanel);
    layout->addWidget(refreshBtn);

    m_stackedWidget->addWidget(page);
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
        "Version 0.3.0 (Phase 3: Boot Readiness Validation)\n\n"
        "An ambitious systems-programming project aiming to run custom Android ROM packages "
        "inside a dynamic virtualization environment on Windows using a modular QEMU backend.\n\n"
        "Phase 3 Features:\n"
        " - Shared Models Module implementation (boot_info, vbmeta_info, payload_info, rom_metadata)\n"
        " - Categorized Logging system\n"
        " - Boot readiness validations (carving offsets, super/payload slot checking, AVB vbmeta verify flag scans)\n"
        " - Generating virtual machine descriptions (boot_config.json containing offsets & vm_profiles)\n"
        " - Native Windows Light/Dark look and feel with full checklist reports\n"
        " - Self-contained automated parser and validator unit testing suite\n\n"
        "Developed by abisin-raj <abisinr201@gmail.com>.",
        this
    );
    aboutText->setWordWrap(true);
    layout->addWidget(aboutText);

    layout->addStretch();
    m_stackedWidget->addWidget(page);
}
