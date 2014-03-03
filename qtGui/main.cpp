#include <signal.h>

#include <QApplication>
#include <QObject>

#include "mainwindow.h"
#include "formant-worker.h"

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
  FormantWorker worker;

  QObject::connect(&worker, &FormantWorker::newFormants,
                   &window, &MainWindow::handleFormants,
                   Qt::DirectConnection);

  QObject::connect(&app, &QCoreApplication::aboutToQuit,
                   &window, &MainWindow::stop);

  QObject::connect(&app, &QCoreApplication::aboutToQuit,
                   &worker, &FormantWorker::stop);

  window.show();
  worker.start();

  return app.exec();
}
