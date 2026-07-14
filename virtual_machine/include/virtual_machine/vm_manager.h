#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <filesystem>
#include <string>

class QemuLauncher;
class EventDispatcher;
class BootPipelineManager;

namespace firmware { class SmcDispatcher; }
namespace qemu_backend { class SmcInterceptor; }

class VmManager : public QObject {
    Q_OBJECT
public:
    explicit VmManager(QObject* parent = nullptr);
    ~VmManager();

    /**
     * @brief Launches the QEMU virtual machine for the specified ROM name.
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

    EventDispatcher* getDispatcher() const { return m_dispatcher; }
    BootPipelineManager* getPipelineManager() const { return m_pipelineManager; }

signals:
    void vmStateChanged(bool running);
    void vmFinished(int exitCode);

private slots:
    void onLauncherStateChanged(bool running);
    void onLauncherFinished(int exitCode);
    void onLogLineRead(const QString& line);

private:
    QemuLauncher*       m_launcher       = nullptr;
    EventDispatcher*    m_dispatcher     = nullptr;
    BootPipelineManager* m_pipelineManager = nullptr;

    // Firmware compatibility layer — intercepts SMC calls via QEMU GDB stub
    firmware::SmcDispatcher*        m_smcDispatcher  = nullptr;
    qemu_backend::SmcInterceptor*   m_smcInterceptor = nullptr;

    std::filesystem::path m_activeRomDir;
    std::string m_activeRomName;
    bool m_isRunning = false;

    QElapsedTimer m_timer;
    QString m_fullLogText;
};
