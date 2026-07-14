#pragma once
#include <QObject>
#include <filesystem>
#include <string>

class QemuLauncher;

class VmManager : public QObject {
    Q_OBJECT
public:
    explicit VmManager(QObject* parent = nullptr);
    ~VmManager();

    /**
     * @brief Launches the QEMU virtual machine for the specified ROM name.
     *        Carves the boot image offsets on-the-fly, starts QEMU as a child,
     *        and sets up cleanup handlers.
     */
    bool startVm(const std::string& romName, const std::filesystem::path& romDir);

    /**
     * @brief Terminates the running virtual machine and cleans up temporary assets.
     */
    void stopVm();

    /**
     * @brief Checks if QEMU is actively executing.
     */
    bool isVmRunning() const;

signals:
    void vmStateChanged(bool running);
    void vmFinished(int exitCode);

private slots:
    void onLauncherStateChanged(bool running);
    void onLauncherFinished(int exitCode);

private:
    QemuLauncher* m_launcher;
    std::filesystem::path m_activeRomDir;
    bool m_isRunning;
};
