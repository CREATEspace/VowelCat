#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

#include "formant.h"
#include "qcustomplot.h"
#include "ui_mainwindow.h"

enum { N_TRACERS = 5 };
enum { TRACER_FIRST = 0 };
enum { TRACER_LAST = N_TRACERS - 1 };

class MainWindow : public QMainWindow
{
  Q_OBJECT

private:
  Ui::MainWindow *ui;
  QCustomPlot *plot;
  QCPGraph *graph;
  QCPItemTracer *tracers[N_TRACERS];

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

public slots:
  void plotFormant(formant_sample_t f2, formant_sample_t f1);

private:
  void setupPlot();
};

#endif // MAINWINDOW_H

// vim: sw=2
