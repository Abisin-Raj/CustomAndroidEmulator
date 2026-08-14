#include "qemu_backend/qemu_launcher.h"
#include "core/logger.h"
#include <QTextStream>

void QemuLauncher::enableGdbStub(quint16 port) {
    m_gdbPort    = port;
    m_gdbEnabled = true;
    spdlog::info("[QEMU] GDB stub enabled on port {}. Guest will start paused (-S).", port);
}

QemuLauncher::QemuLauncher(QObject* parent)
    : QObject(parent), m_process(new QProcess(this)) {
    connect(m_process, &QProcess::readyReadStandardOutput, this, &QemuLauncher::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &QemuLauncher::onReadyReadStandardError);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &QemuLauncher::onProcessFinished);
}

QemuLauncher::~QemuLauncher() {
    stop();
}

bool QemuLauncher::start(const std::string& qemuPath, const QStringList& args) {
    if (isRunning()) {
        LOG_QEMU_WARN("QemuLauncher: QEMU is already running. Please terminate the active session first.");
        return false;
    }

    // Inject GDB stub arguments when interception is enabled:
    // -gdb tcp::PORT instructs QEMU to listen for a GDB connection
    // -S pauses the guest at startup until the GDB client sends 'continue'
    QStringList effectiveArgs = args;
    if (m_gdbEnabled) {
        effectiveArgs << "-gdb" << QString("tcp::%1").arg(m_gdbPort)
                      << "-S";
        LOG_QEMU_INFO("QemuLauncher: GDB interception active — QEMU will pause for GDB on port {}", m_gdbPort);
    }

    LOG_QEMU_INFO("QemuLauncher: Executing: {} {}", qemuPath, effectiveArgs.join(" ").toStdString());
    
    m_process->start(QString::fromStdString(qemuPath), effectiveArgs);
    if (!m_process->waitForStarted(3000)) {
        LOG_QEMU_ERROR("QemuLauncher: Failed to launch QEMU process: {}", m_process->errorString().toStdString());
        return false;
    }

    LOG_QEMU_INFO("QemuLauncher: Process spawned successfully (PID: {})", m_process->processId());
    emit stateChanged(true);
    return true;
}

void QemuLauncher::stop() {
    if (isRunning()) {
        LOG_QEMU_INFO("QemuLauncher: Terminating active virtual machine process...");
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            LOG_QEMU_WARN("QemuLauncher: QEMU did not close gracefully. Killing process...");
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }
}

bool QemuLauncher::isRunning() const {
    return m_process && m_process->state() == QProcess::Running;
}

void QemuLauncher::writeData(const QByteArray& data) {
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->write(data);
    }
}

void QemuLauncher::onReadyReadStandardOutput() {
    QByteArray data = m_process->readAllStandardOutput();
    QTextStream stream(&data);
    QString line;
    while (stream.readLineInto(&line)) {
        LOG_QEMU_INFO("{}", line.toStdString());
        emit logLineRead(line);
    }
}

void QemuLauncher::onReadyReadStandardError() {
    QByteArray data = m_process->readAllStandardError();
    QTextStream stream(&data);
    QString line;
    while (stream.readLineInto(&line)) {
        LOG_QEMU_WARN("{}", line.toStdString());
        emit logLineRead(line);
    }
}

void QemuLauncher::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    LOG_QEMU_INFO("QemuLauncher: Process terminated with exit code {} ({})", 
        exitCode, exitStatus == QProcess::NormalExit ? "Normal Exit" : "Crash Exit");
    emit stateChanged(false);
    emit finished(exitCode);
}
