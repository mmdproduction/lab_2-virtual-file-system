#include <QApplication>

#include <exception>
#include <iostream>

#include "Gui/MainWindow.hpp"
#include "Gui/UiTheme.hpp"

int main(int argc, char* argv[]) {
#ifdef Q_OS_LINUX
    qputenv("QT_QPA_PLATFORMTHEME", "generic");
#endif
    QApplication application(argc, argv);
    QApplication::setApplicationName("VFS");
    QApplication::setStyle("Fusion");
    application.setPalette(ui::DarkPalette());
    try {
        MainWindow window;
        window.show();
        return application.exec();
    } catch (const std::exception& error) {
        std::cerr << "Cannot start VFS: " << error.what() << '\n';
        return 1;
    }
}
