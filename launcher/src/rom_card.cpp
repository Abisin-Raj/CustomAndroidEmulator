#include "rom_card.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

RomCard::RomCard(const RomMetadata& meta, QWidget* parent)
    : QWidget(parent), m_meta(meta) {
    setObjectName("RomCard");
    setupUi();
}

void RomCard::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    // Header layout: Title and Status Badge
    auto* headerLayout = new QHBoxLayout();
    
    auto* nameLabel = new QLabel(QString::fromStdString(m_meta.name), this);
    nameLabel->setObjectName("RomName");
    headerLayout->addWidget(nameLabel);
    headerLayout->addStretch();

    auto* statusLabel = new QLabel(QString::fromStdString(m_meta.status), this);
    statusLabel->setObjectName("StatusBadge");
    headerLayout->addWidget(statusLabel);

    mainLayout->addLayout(headerLayout);

    // Details layout (Grid or vertical)
    auto* detailsLayout = new QVBoxLayout();
    detailsLayout->setSpacing(4);

    auto* deviceLabel = new QLabel(QString("Device: %1").arg(QString::fromStdString(m_meta.device)), this);
    deviceLabel->setObjectName("RomDetail");
    detailsLayout->addWidget(deviceLabel);

    auto* versionLabel = new QLabel(QString("Android Version: %1").arg(QString::fromStdString(m_meta.androidVersion)), this);
    versionLabel->setObjectName("RomDetail");
    detailsLayout->addWidget(versionLabel);

    auto* archLabel = new QLabel(QString("Architecture: %1").arg(QString::fromStdString(m_meta.architecture)), this);
    archLabel->setObjectName("RomDetail");
    detailsLayout->addWidget(archLabel);

    mainLayout->addLayout(detailsLayout);
    mainLayout->addSpacing(6);

    // Actions layout: Launch, Info, Delete
    auto* actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(8);

    auto* launchBtn = new QPushButton("Launch", this);
    launchBtn->setObjectName("ActionButton");
    launchBtn->setEnabled(false);
    launchBtn->setToolTip("Virtualization engine is not integrated in Phase 1");
    actionsLayout->addWidget(launchBtn);

    auto* infoBtn = new QPushButton("Info", this);
    infoBtn->setObjectName("ActionButton");
    connect(infoBtn, &QPushButton::clicked, this, [this]() {
        emit infoRequested(m_meta);
    });
    actionsLayout->addWidget(infoBtn);

    auto* deleteBtn = new QPushButton("Delete", this);
    deleteBtn->setObjectName("DeleteButton");
    connect(deleteBtn, &QPushButton::clicked, this, [this]() {
        emit uninstallRequested(m_meta.name);
    });
    actionsLayout->addWidget(deleteBtn);

    mainLayout->addLayout(actionsLayout);

    // QSS styling specific to RomCard
    setStyleSheet(R"(
        QWidget#RomCard {
            background-color: #1e1e24;
            border: 1px solid #2e2e38;
            border-radius: 12px;
        }
        QWidget#RomCard:hover {
            background-color: #24242c;
            border: 1px solid #00e5ff;
        }
        QLabel#RomName {
            color: #ffffff;
            font-size: 15px;
            font-weight: bold;
        }
        QLabel#RomDetail {
            color: #a0a0b0;
            font-size: 12px;
        }
        QLabel#StatusBadge {
            background-color: rgba(0, 229, 255, 0.1);
            color: #00e5ff;
            border: 1px solid rgba(0, 229, 255, 0.3);
            border-radius: 4px;
            padding: 2px 8px;
            font-size: 11px;
            font-weight: bold;
        }
        QPushButton#ActionButton {
            background-color: #2c2c35;
            border: 1px solid #3c3c45;
            border-radius: 6px;
            color: #ffffff;
            padding: 6px 12px;
            font-size: 12px;
            font-weight: bold;
        }
        QPushButton#ActionButton:hover {
            background-color: #383845;
            border: 1px solid #00e5ff;
        }
        QPushButton#ActionButton:disabled {
            background-color: #141418;
            border: 1px solid #202024;
            color: #555560;
        }
        QPushButton#DeleteButton {
            background-color: rgba(255, 23, 68, 0.08);
            border: 1px solid rgba(255, 23, 68, 0.25);
            border-radius: 6px;
            color: #ff1744;
            padding: 6px 12px;
            font-size: 12px;
            font-weight: bold;
        }
        QPushButton#DeleteButton:hover {
            background-color: rgba(255, 23, 68, 0.18);
            border: 1px solid #ff1744;
        }
    )");
}
