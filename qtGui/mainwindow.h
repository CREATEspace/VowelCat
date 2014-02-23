#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include "qcustomplot.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

private slots:
  void bracketDataSlot();

private:
  void setupPlot(QCustomPlot *customPlot);

  QVector<double> x, y;
  Ui::MainWindow *ui;
  QTimer dataTimer;
  QCPItemTracer *tracer;
  QCPItemTracer *laggingTracer;
  QCPItemTracer *laggingTracer2;
  int frame;
};

#endif // MAINWINDOW_H

// vim: sw=2
