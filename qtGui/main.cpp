#include <signal.h>
#include <stdlib.h>

#include <QApplication>

#include "mainwindow.h"
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
  worker_t worker;

  window.show();

  worker_init(&worker, &window);
  worker_start(&worker);

  // This function blocks until the main window is closed or
  // QCoreApplication::quit is called.
  QCoreApplication::exec();

  worker_stop(&worker);
  worker_destroy(&worker);
}
