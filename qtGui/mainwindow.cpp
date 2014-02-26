#include <inttypes.h>
#include <math.h>

#include <QColor>
#include <QDebug>
#include <QDesktopWidget>
#include <QScreen>
#include <QMessageBox>
#include <QMetaEnum>

#include "formant.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

using namespace std;

double Tracer::size(double x) {
    return DIAM_MIN + DIAM_RANGE * x / Tracer::LAST;
}

uint32_t Tracer::alpha(double x) {
    return ALPHA_MIN + pow(ALPHA_RANGE + 1, x / Tracer::LAST) - 1;
}

uint32_t Tracer::rgba(double x) {
    return alpha(x) << 24 | COLOR_BASE;
}

Tracer::Tracer(QCustomPlot *plot, QCPGraph *graph, size_t i):
    QCPItemTracer(plot)
{
    setGraph(graph);
    setStyle(QCPItemTracer::tsCircle);
    setPen(Qt::NoPen);
    setBrush(QColor::fromRgba(rgba(i)));
    setSize(size(i));
}

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  setGeometry(400, 250, 1000, 800);

  plot = ui->customPlot;
  graph = plot->addGraph();

  setupPlot();
  plot->replot();
}

void MainWindow::setupPlot()
{
  plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
  graph->setPen(Qt::NoPen);

  plot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
  plot->xAxis->setRange(2400, 700);
  plot->xAxis->setRangeReversed(true);
  plot->xAxis->setLabel("F2 (Hz)");

  plot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
  plot->yAxis->setRange(900, 250);
  plot->yAxis->setRangeReversed(true);
  plot->yAxis->setLabel("F1 (Hz)");

  QCPItemText *upperHighBackRounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(upperHighBackRounded);
  upperHighBackRounded->position->setCoords(750, 295);
  upperHighBackRounded->setText("u");
  upperHighBackRounded->setFont(QFont(font().family(), 20));

  QCPItemText *lowerHighBackRounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(lowerHighBackRounded);
  lowerHighBackRounded->position->setCoords(910, 334);
  lowerHighBackRounded->setText("ʊ");
  lowerHighBackRounded->setFont(QFont(font().family(), 20));

  QCPItemText *upperMidBackRounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(upperMidBackRounded);
  upperMidBackRounded->position->setCoords(727, 406);
  upperMidBackRounded->setText("o");
  upperMidBackRounded->setFont(QFont(font().family(), 20));

  QCPItemText *lowerMidBackRounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(lowerMidBackRounded);
  lowerMidBackRounded->position->setCoords(830, 541);
  lowerMidBackRounded->setText("ɔ");
  lowerMidBackRounded->setFont(QFont(font().family(), 20));

  QCPItemText *lowerLowBackRounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(lowerLowBackRounded);
  lowerLowBackRounded->position->setCoords(843, 652);
  lowerLowBackRounded->setText("ɒ");
  lowerLowBackRounded->setFont(QFont(font().family(), 20));

  QCPItemText *lowerLowBackUnrounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(lowerLowBackUnrounded);
  lowerLowBackUnrounded->position->setCoords(1065, 781);
  lowerLowBackUnrounded->setText("ɑ");
  lowerLowBackUnrounded->setFont(QFont(font().family(), 20));

  for (size_t i = 0; i < Tracer::COUNT; i += 1)
    tracers[i] = new Tracer(plot, graph, i);
}

void MainWindow::plotFormant(formant_sample_t f2, formant_sample_t f1) {
  graph->removeData(tracers[0]->graphKey());
  graph->addData(f2, f1);

  for (size_t i = 0; i < Tracer::LAST; i += 1)
    tracers[i]->setGraphKey(tracers[i + 1]->graphKey());

  tracers[Tracer::LAST]->setGraphKey(f2);
  plot->replot();
}

MainWindow::~MainWindow()
{
  delete ui;
}
