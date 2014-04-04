// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#include <signal.h>
#include <stdlib.h>

#include <QApplication>

#include "mainwindow.h"
#include "plotter.h"

// Handle OS signals.
static void sig(int s) {
    switch (s) {
    case SIGINT:
    case SIGTERM:
        QCoreApplication::quit();
    }
}

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    QApplication::setGraphicsSystem("raster");
#endif

    signal(SIGINT, sig);
    signal(SIGTERM, sig);

    QApplication app(argc, argv);
    MainWindow window;
    plotter_t plotter;

    window.show();

    plotter_init(&plotter, &window);
    plotter_start(&plotter);

    // This function blocks until the main window is closed or
    // QCoreApplication::quit is called.
    QCoreApplication::exec();

    plotter_stop(&plotter);
    plotter_destroy(&plotter);
}
