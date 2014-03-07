#include <signal.h>
#include <stdlib.h>

#include <QApplication>

#include "mainwindow.h"
#include "plotter.h"
#include "worker.h"

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
  worker_t worker;

  window.show();

  plotter_init(&plotter, &window);
  worker_init(&worker, &window, &plotter);

  plotter_start(&plotter);
  worker_start(&worker);

  // This function blocks until the main window is closed or
  // QCoreApplication::quit is called.
  QCoreApplication::exec();

  worker_stop(&worker);
  plotter_stop(&plotter);

  worker_destroy(&worker);
  plotter_destroy(&plotter);
}
