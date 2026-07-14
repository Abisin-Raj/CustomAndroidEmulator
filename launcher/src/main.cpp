#include <QApplication>
#include <spdlog/spdlog.h>
#include "mainwindow.h"

int main(int argc, char* argv[]) {
    // Configure spdlog logging pattern
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    spdlog::info("Starting CustomAndroidEmulator launcher...");

    QApplication app(argc, argv);
    
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
