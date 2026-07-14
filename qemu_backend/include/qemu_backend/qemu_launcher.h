#pragma once
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <string>

class QemuLauncher : public QObject {
    Q_OBJECT
public:
    explicit QemuLauncher(QObject* parent = nullptr);
    ~QemuLauncher();

    bool start(const std::string& qemuPath, const QStringList& args);
    void stop();
    bool isRunning() const;

signals:
    void stateChanged(bool running);
    void finished(int exitCode);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess* m_process;
};
