// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#include <signal.h>
#include <stdlib.h>

#include <QApplication>

extern "C" {
    #include "audio.h"
}

#include "mainwindow.h"
#include "plotter.h"
#include "params.h"


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

    audio_t audio;
    audio_init(&audio, SAMPLE_RATE, CHANNELS, SAMPLES_PER_CHUNK);

    Plotter plotter(&audio);
    MainWindow window(&audio);
    
    plotter.window = &window;
    window.plotter = &plotter;

    QObject::connect(&plotter, SIGNAL(pauseSig()), &window, SLOT(pauseAudio()), Qt::QueuedConnection);

    plotter.listen();
    window.show();

    // This function blocks until the main window is closed or
    // QCoreApplication::quit is called.
    QCoreApplication::exec();

    audio_destroy(&audio);
}
