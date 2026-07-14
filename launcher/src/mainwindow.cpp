#include "mainwindow.h"
#include "rom_card.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressDialog>
#include <QCoreApplication>
#include <QPushButton>
#include <QGroupBox>
#include <spdlog/spdlog.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("Custom Android Emulator");
    resize(960, 600);
    setMinimumSize(800, 500);

    // Initializing RomManager pointing to "roms" directory in application path
    QString romsDir = QCoreApplication::applicationDirPath() + "/roms";
    spdlog::info("ROMs base directory: {}", romsDir.toStdString());
    m_romManager = std::make_unique<RomManager>(romsDir.toStdString());

    setupUi();
    applyTheme();
    loadInstalledRoms();
}

void MainWindow::setupUi() {
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Sidebar Layout (Left Pane)
    auto* sidebarWidget = new QWidget(this);
    sidebarWidget->setObjectName("Sidebar");
    sidebarWidget->setFixedWidth(260);

    auto* sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(24, 30, 24, 30);
    sidebarLayout->setSpacing(16);

    // App Brand Logo/Header
    auto* brandLayout = new QVBoxLayout();
    brandLayout->setSpacing(4);
    
    auto* logoLabel = new QLabel("Antigravity", this);
    logoLabel->setObjectName("BrandLogo");
    logoLabel->setStyleSheet("font-size: 12px; font-weight: bold; color: #00e5ff; letter-spacing: 2px; text-transform: uppercase;");
    brandLayout->addWidget(logoLabel);

    auto* titleLabel = new QLabel("ROM Launcher", this);
    titleLabel->setObjectName("HeaderTitle");
    titleLabel->setStyleSheet("font-size: 22px; font-weight: 800; color: #ffffff;");
    brandLayout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel("v0.1.0 (Foundation)", this);
    subtitleLabel->setObjectName("HeaderSubtitle");
    subtitleLabel->setStyleSheet("font-size: 11px; color: #6a6a7a;");
    brandLayout->addWidget(subtitleLabel);

    sidebarLayout->addLayout(brandLayout);
    sidebarLayout->addSpacing(30);

    // Sidebar Action Buttons
    auto* installBtn = new QPushButton("Install ROM", this);
    installBtn->setObjectName("SidebarButton");
    connect(installBtn, &QPushButton::clicked, this, &MainWindow::onInstallRomClicked);
    sidebarLayout->addWidget(installBtn);

    auto* settingsBtn = new QPushButton("Settings", this);
    settingsBtn->setObjectName("SidebarButton");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    sidebarLayout->addWidget(settingsBtn);

    sidebarLayout->addStretch();

    auto* exitBtn = new QPushButton("Exit Application", this);
    exitBtn->setObjectName("ExitButton");
    connect(exitBtn, &QPushButton::clicked, this, &MainWindow::close);
    sidebarLayout->addWidget(exitBtn);

    mainLayout->addWidget(sidebarWidget);

    // Content Layout (Right Pane)
    auto* contentWidget = new QWidget(this);
    contentWidget->setObjectName("ContentPane");
    
    auto* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(30, 30, 30, 30);
    contentLayout->setSpacing(20);

    // Section title
    auto* sectionTitle = new QLabel("Installed ROMs", this);
    sectionTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #ffffff;");
    contentLayout->addWidget(sectionTitle);

    // Scroll Area for ROM cards
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    
    m_cardsContainer = new QWidget(m_scrollArea);
    m_cardsContainer->setObjectName("CardsContainer");
    
    m_cardsLayout = new QGridLayout(m_cardsContainer);
    m_cardsLayout->setContentsMargins(4, 4, 4, 4);
    m_cardsLayout->setSpacing(16);
    
    m_scrollArea->setWidget(m_cardsContainer);
    contentLayout->addWidget(m_scrollArea);

    // No ROMs Status Label
    m_noRomsLabel = new QLabel("No ROMs Installed\n\nClick 'Install ROM' in the sidebar to add a custom ROM ZIP package.", this);
    m_noRomsLabel->setAlignment(Qt::AlignCenter);
    m_noRomsLabel->setObjectName("NoRomsLabel");
    m_noRomsLabel->setStyleSheet("font-size: 14px; color: #7a7a8a; line-height: 1.5;");
    contentLayout->addWidget(m_noRomsLabel);

    mainLayout->addWidget(contentWidget);
}

void MainWindow::applyTheme() {
    setStyleSheet(R"(
        QMainWindow {
            background-color: #0f0f12;
        }
        QWidget#Sidebar {
            background-color: #15151c;
            border-right: 1px solid #20202a;
        }
        QWidget#ContentPane {
            background-color: #0f0f12;
        }
        QWidget#CardsContainer {
            background-color: transparent;
        }
        QScrollArea {
            border: none;
            background-color: transparent;
        }
        QPushButton#SidebarButton {
            background-color: #1e1e26;
            border: 1px solid #2d2d3a;
            border-radius: 8px;
            color: #ffffff;
            padding: 12px 18px;
            font-size: 13px;
            font-weight: bold;
            text-align: left;
        }
        QPushButton#SidebarButton:hover {
            background-color: #262634;
            border: 1px solid #00e5ff;
        }
        QPushButton#ExitButton {
            background-color: rgba(255, 23, 68, 0.08);
            border: 1px solid rgba(255, 23, 68, 0.25);
            border-radius: 8px;
            color: #ff1744;
            padding: 12px 18px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton#ExitButton:hover {
            background-color: rgba(255, 23, 68, 0.18);
            border: 1px solid #ff1744;
        }
        
        /* Scrollbar styling */
        QScrollBar:vertical {
            background-color: #121216;
            width: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background-color: #3e3e4a;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #00e5ff;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )");
}

void MainWindow::loadInstalledRoms() {
    // Clear existing card widgets
    QLayoutItem* child;
    while ((child = m_cardsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    std::vector<RomMetadata> roms = m_romManager->getInstalledRoms();
    spdlog::info("Loaded {} installed ROMs", roms.size());

    if (roms.empty()) {
        m_noRomsLabel->setVisible(true);
        m_scrollArea->setVisible(false);
    } else {
        m_noRomsLabel->setVisible(false);
        m_scrollArea->setVisible(true);

        int columnCount = 2; // Display 2 columns of cards
        for (size_t i = 0; i < roms.size(); ++i) {
            auto* card = new RomCard(roms[i], m_cardsContainer);
            connect(card, &RomCard::uninstallRequested, this, &MainWindow::onUninstallRequested);
            connect(card, &RomCard::infoRequested, this, &MainWindow::onInfoRequested);

            int row = static_cast<int>(i) / columnCount;
            int col = static_cast<int>(i) % columnCount;
            m_cardsLayout->addWidget(card, row, col);
        }
        
        // Add stretch row at the end to keep cards top-aligned
        m_cardsLayout->setRowStretch(static_cast<int>((roms.size() - 1) / columnCount) + 1, 1);
    }
}

void MainWindow::onInstallRomClicked() {
    QString zipPath = QFileDialog::getOpenFileName(this, "Select Custom ROM ZIP", "", "Android ROM ZIPs (*.zip)");
    if (zipPath.isEmpty()) return;

    RomMetadata meta;
    std::string error;
    if (!m_romManager->validateRomZip(zipPath.toStdString(), meta, error)) {
        QMessageBox::critical(this, "Invalid ROM Package", 
            QString("The selected ZIP file is not a valid Android custom ROM package.\n\nError: %1").arg(QString::fromStdString(error)));
        return;
    }

    QString details = QString(
        "A valid custom ROM package was detected!\n\n"
        "Device: %1\n"
        "Android Version: %2\n"
        "Architecture: %3\n\n"
        "Would you like to install it now?\n"
        "(This will extract partition images into your local workspace)"
    ).arg(QString::fromStdString(meta.device), QString::fromStdString(meta.androidVersion), QString::fromStdString(meta.architecture));

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm ROM Installation", details, QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        bool ok;
        QString defaultName = QString::fromStdString(meta.name);
        QString romName = QInputDialog::getText(
            this, "ROM Name", "Enter a custom name for this ROM installation:", 
            QLineEdit::Normal, defaultName, &ok
        );
        if (!ok || romName.trimmed().isEmpty()) return;

        QProgressDialog progress("Extracting ROM images, please wait...", nullptr, 0, 0, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(0);
        progress.show();
        QCoreApplication::processEvents();

        std::string installError;
        bool success = m_romManager->installRom(zipPath.toStdString(), romName.trimmed().toStdString(), installError);
        progress.close();

        if (success) {
            QMessageBox::information(this, "Installation Complete", 
                QString("ROM '%1' has been successfully installed!").arg(romName));
            loadInstalledRoms();
        } else {
            QMessageBox::critical(this, "Installation Failed", 
                QString("An error occurred during ROM installation:\n\n%1").arg(QString::fromStdString(installError)));
        }
    }
}

void MainWindow::onSettingsClicked() {
    QString romsDir = QCoreApplication::applicationDirPath() + "/roms";
    QString settingsInfo = QString(
        "Custom Android Emulator Settings\n\n"
        "ROMs Directory:\n%1\n\n"
        "Later phases will allow configuring QEMU paths, partition allocations, and virtual CPU cores."
    ).arg(romsDir);

    QMessageBox::information(this, "Settings", settingsInfo);
}

void MainWindow::onUninstallRequested(const std::string& romName) {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Uninstall ROM", 
        QString("Are you sure you want to uninstall '%1'?\nThis will permanently delete all extracted image files for this ROM.").arg(QString::fromStdString(romName)),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        std::string error;
        if (m_romManager->uninstallRom(romName, error)) {
            QMessageBox::information(this, "Uninstalled", QString("ROM '%1' has been uninstalled.").arg(QString::fromStdString(romName)));
            loadInstalledRoms();
        } else {
            QMessageBox::critical(this, "Error", QString("Failed to uninstall ROM:\n\n%1").arg(QString::fromStdString(error)));
        }
    }
}

void MainWindow::onInfoRequested(const RomMetadata& meta) {
    QString info = QString(
        "ROM Details:\n\n"
        "Name: %1\n"
        "Device: %2\n"
        "Android Version: %3\n"
        "Architecture: %4\n"
        "Status: %5\n\n"
        "Location: %6/roms/%1"
    ).arg(
        QString::fromStdString(meta.name),
        QString::fromStdString(meta.device),
        QString::fromStdString(meta.androidVersion),
        QString::fromStdString(meta.architecture),
        QString::fromStdString(meta.status),
        QCoreApplication::applicationDirPath()
    );

    QMessageBox::information(this, "ROM Details", info);
}
