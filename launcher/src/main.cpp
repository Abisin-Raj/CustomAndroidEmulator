#include <QApplication>
#include "core/logger.h"
#include "mainwindow.h"

int main(int argc, char* argv[]) {
    // Initialize shared logging system
    core::initLogger();
    LOG_INFO("Initializing launcher UI...");

    QApplication app(argc, argv);

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
