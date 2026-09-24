#include <QApplication>
#include "core/logger.h"
#include "mainwindow.h"

int main(int argc, char* argv[]) {
    // Initialize shared logging system
    core::initLogger();
    LOG_LAUNCHER_INFO("Initializing launcher UI...");

    QApplication app(argc, argv);

    MainWindow mainWindow;
    mainWindow.show();

    // Support --autoboot <rom_name> argument to start VM without manual GUI clicks
    QStringList args = QApplication::arguments();
    int autobootIndex = args.indexOf("--autoboot");
    if (autobootIndex != -1 && autobootIndex + 1 < args.size()) {
        QString romName = args.at(autobootIndex + 1);
        mainWindow.autoLaunchRom(romName);
    }

    return app.exec();
}
