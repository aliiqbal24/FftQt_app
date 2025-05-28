#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow window; // call main window cpp
    window.show(); // display it

    return app.exec();
}
