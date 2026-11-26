// ScreenCTR — Two screens. One connection.
// Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "MainWindow.h"
#include "Settings.h"
#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("ScreenCTR");
    QApplication::setOrganizationName("Heivoli Network");
    QApplication::setApplicationVersion(SCREENCTR_VERSION);
    QApplication::setWindowIcon(QIcon(":/icon.png"));
    QApplication::setStyle("Fusion");

    QFile qss(":/style.qss");
    if (qss.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    Settings::get().load();
    Settings::get().save(); // persists the generated client id on first run

    MainWindow w;
    w.show();
    return app.exec();
}
