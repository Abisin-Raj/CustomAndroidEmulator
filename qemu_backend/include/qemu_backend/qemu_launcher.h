#pragma once
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <string>
#include <cstdint>

class QemuLauncher : public QObject {
    Q_OBJECT
public:
    explicit QemuLauncher(QObject* parent = nullptr);
    ~QemuLauncher();

    // Enable GDB stub interception. When set, QEMU will be started with:
    //   -gdb tcp::port -S
    // The guest is paused at start; caller must connect a GDB client and
    // send 'continue' before the guest executes any code.
    void enableGdbStub(quint16 port);
    quint16 gdbPort() const { return m_gdbPort; }
    bool isGdbEnabled() const { return m_gdbEnabled; }

    bool start(const std::string& qemuPath, const QStringList& args);
    void stop();
    bool isRunning() const;

signals:
    void stateChanged(bool running);
    void finished(int exitCode);
    void logLineRead(const QString& line);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess* m_process;
    quint16   m_gdbPort    = 1234;
    bool      m_gdbEnabled = false;
};

