#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

#include "qcustomplot.h"
#include "ui_mainwindow.h"

enum { N_TRACERS = 5 };
enum { TRACER_FIRST = 0 };
enum { TRACER_LAST = N_TRACERS - 1 };

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

private slots:
  void bracketDataSlot();

private:
  void setupPlot();

  QCPItemTracer *tracers[N_TRACERS];

  QVector<double> x, y;
  Ui::MainWindow *ui;
  QCustomPlot *plot;
  QCPGraph *graph;
  QTimer dataTimer;
  int frame;
};

#endif // MAINWINDOW_H

// vim: sw=2
