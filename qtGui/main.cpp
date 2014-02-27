#include <QApplication>
#include <QObject>

#include "mainwindow.h"
#include "formant-worker.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
  QApplication::setGraphicsSystem("raster");
#endif

  QApplication app(argc, argv);
  MainWindow window;
  FormantWorker worker;

  QObject::connect(&worker, &FormantWorker::newFormant,
                   &window, &MainWindow::plotFormant,
                   Qt::QueuedConnection);

  window.show();
  worker.start();

  return app.exec();
}
