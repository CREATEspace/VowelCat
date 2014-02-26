#include <QColor>
#include <QDebug>
#include <QDesktopWidget>
#include <QScreen>
#include <QMessageBox>
#include <QMetaEnum>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

#include "formant.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

using namespace std;

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow),
  frame(0)
{
  ui->setupUi(this);
  setGeometry(400, 250, 1000, 800);

  plot = ui->customPlot;
  graph = plot->addGraph();

  setupPlot();
  plot->replot();
}

static double tracerSize(double x) {
  enum { TRACER_SIZE_MAX = 40 };
  enum { TRACER_SIZE_MIN = 14 };
  enum { TRACER_SIZE_RANGE = TRACER_SIZE_MAX - TRACER_SIZE_MIN };

  return TRACER_SIZE_RANGE * x / N_TRACERS + TRACER_SIZE_MIN;
}

static double tracerAlpha(double x) {
  enum { TRACER_ALPHA_MAX = 255 };
  enum { TRACER_ALPHA_MIN = 64 };

  return pow(TRACER_ALPHA_MAX, x / TRACER_LAST);
}

void MainWindow::setupPlot()
{
  int xpoint;
  int ypoint;
  ifstream points;

  vector<string> pointFiles;
  pointFiles.push_back("pointData/oooo.points");
  pointFiles.push_back("pointData/eeee.points");
  pointFiles.push_back("pointData/augh.points");
  pointFiles.push_back("pointData/ae.points");

  for (uint i = 0; i < pointFiles.size(); i++){
    points.open(pointFiles[i].c_str());
    if (points.is_open()) {
      points >> xpoint;
      points >> ypoint;
      while (!points.eof()){
        x.append(xpoint);
        y.append(ypoint);
        points >> xpoint;
        points >> ypoint;
      }
      points.close();
    }
  }

  plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
  graph->setData(x, y);
  graph->setPen(Qt::NoPen);
  graph->rescaleKeyAxis();

  plot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
  plot->xAxis->setRange(3000, 0);
  plot->xAxis->setRangeReversed(true);
  plot->xAxis->setLabel("F2 (Hz)");

  plot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
  plot->yAxis->setRange(3000, 0);
  plot->yAxis->setRangeReversed(true);
  plot->yAxis->setLabel("F1 (Hz)");

  for (size_t i = 0; i < N_TRACERS; i += 1) {
    tracers[i] = new QCPItemTracer(plot);
    tracers[i]->setGraph(graph);
    tracers[i]->setInterpolating(false);
    tracers[i]->setStyle(QCPItemTracer::tsCircle);
    tracers[i]->setPen(Qt::NoPen);
    QColor color(Qt::red);
    color.setAlpha(tracerAlpha(i));
    tracers[i]->setBrush(color);
    tracers[i]->setSize(tracerSize(i));
  }

  // setup a timer that repeatedly calls MainWindow::realtimeDataSlot:
  connect(&dataTimer, SIGNAL(timeout()), this, SLOT(bracketDataSlot()));
  dataTimer.start(100); // Interval 0 means to refresh as fast as possible
}

void MainWindow::bracketDataSlot()
{
  double key = x[frame % x.size()];
  double val = y[frame % y.size()];

  plotFormant(key, val);
  frame += 1;
}

void MainWindow::plotFormant(formant_sample_t f2, formant_sample_t f1) {
  graph->removeData(tracers[TRACER_FIRST]->graphKey());
  graph->addData(f2, f1);

  for (size_t i = TRACER_FIRST; i < TRACER_LAST; i += 1)
    tracers[i]->setGraphKey(tracers[i + 1]->graphKey());

  tracers[TRACER_LAST]->setGraphKey(f2);
  plot->replot();
}

MainWindow::~MainWindow()
{
  delete ui;
}

// vim: sw=2
