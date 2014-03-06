#include <inttypes.h>
#include <math.h>
#include <time.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include <QColor>
#include <QObject>
#include <QTimer>
#include <QWidget>

#include "formant.h"
#include "mainwindow.h"
#include "timespec.h"
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
  pairs(NULL),
  nsec_per_pair(UINTMAX_MAX),
  ui(new Ui::MainWindow)
{
  // Start at the origin for lack of a better place.
  cur = (pair_t) {
      .x = 0,
      .y = 0,
  };

  ui->setupUi(this);
  setGeometry(400, 250, 1000, 800);

  plot = ui->customPlot;
  graph = plot->addGraph();

  QObject::connect(&timer, &QTimer::timeout,
                   this, &MainWindow::plotFormant);

  setupPlot();
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
  upperHighBackRounded->setFont(QFont(font().family(), 30));

  QCPItemText *lowerHighBackRounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(lowerHighBackRounded);
  lowerHighBackRounded->position->setCoords(910, 334);
  lowerHighBackRounded->setText("ʊ");
  lowerHighBackRounded->setFont(QFont(font().family(), 30));

  QCPItemText *upperMidBackRounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(upperMidBackRounded);
  upperMidBackRounded->position->setCoords(727, 406);
  upperMidBackRounded->setText("o");
  upperMidBackRounded->setFont(QFont(font().family(), 30));

  QCPItemText *lowerMidBackRounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(lowerMidBackRounded);
  lowerMidBackRounded->position->setCoords(830, 541);
  lowerMidBackRounded->setText("ɔ");
  lowerMidBackRounded->setFont(QFont(font().family(), 30));

  QCPItemText *lowerLowBackRounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(lowerLowBackRounded);
  lowerLowBackRounded->position->setCoords(843, 652);
  lowerLowBackRounded->setText("ɒ");
  lowerLowBackRounded->setFont(QFont(font().family(), 30));

  QCPItemText *lowerLowBackUnrounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(lowerLowBackUnrounded);
  lowerLowBackUnrounded->position->setCoords(1065, 781);
  lowerLowBackUnrounded->setText("ɑ");
  lowerLowBackUnrounded->setFont(QFont(font().family(), 30));

  QCPItemText *lowerLowCentralUnrounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(lowerLowCentralUnrounded);
  lowerLowCentralUnrounded->position->setCoords(1211, 784);
  lowerLowCentralUnrounded->setText("ä");
  lowerLowCentralUnrounded->setFont(QFont(font().family(), 30));

  QCPItemText *lowerLowFrontUnrounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(lowerLowFrontUnrounded);
  lowerLowFrontUnrounded->position->setCoords(1632, 806);
  lowerLowFrontUnrounded->setText("a");
  lowerLowFrontUnrounded->setFont(QFont(font().family(), 30));

  QCPItemText *upperLowFrontUnrounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(upperLowFrontUnrounded);
  upperLowFrontUnrounded->position->setCoords(1782, 766);
  upperLowFrontUnrounded->setText("æ");
  upperLowFrontUnrounded->setFont(QFont(font().family(), 30));

  QCPItemText *lowerMidFrontUnrounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(lowerMidFrontUnrounded);
  lowerMidFrontUnrounded->position->setCoords(1840, 541);
  lowerMidFrontUnrounded->setText("ɛ");
  lowerMidFrontUnrounded->setFont(QFont(font().family(), 30));

  QCPItemText *upperMidFrontUnrounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(upperMidFrontUnrounded);
  upperMidFrontUnrounded->position->setCoords(2148, 434);
  upperMidFrontUnrounded->setText("e");
  upperMidFrontUnrounded->setFont(QFont(font().family(), 30));

  QCPItemText *lowerHighFrontUnrounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(lowerHighFrontUnrounded);
  lowerHighFrontUnrounded->position->setCoords(2187, 360);
  lowerHighFrontUnrounded->setText("ɪ");
  lowerHighFrontUnrounded->setFont(QFont(font().family(), 30));

  QCPItemText *upperHighFrontUnrounded = new QCPItemText(ui->customPlot);
  ui->customPlot->addItem(upperHighFrontUnrounded);
  upperHighFrontUnrounded->position->setCoords(2343, 294);
  upperHighFrontUnrounded->setText("i");
  upperHighFrontUnrounded->setFont(QFont(font().family(), 30));

  for (size_t i = 0; i < Tracer::COUNT; i += 1)
    tracers[i] = new Tracer(plot, graph, i);
}

void MainWindow::handleFormants(const sound_t *formants, uintmax_t nsec) {
    // Prevent any more running of plotFormant.
    timer.stop();

    // This array should never need to be reallocated after the first
    // allocation, but better safe than sorry I guess.
    pair_count = formants->n_samples;
    pairs = (pair_t *) realloc(pairs, pair_count * sizeof(pair_t));

    // Copy the raw formants into our pairs structure.
    for (size_t i = 0; i < pair_count; i += 1) {
        pairs[i] = (pair_t) {
            .x = sound_get_f2(formants, i),
            .y = sound_get_f1(formants, i),
        };
    }

    // Calculate how long a pair should be tweened between before moving on to
    // the next.
    nsec_per_pair = nsec / pair_count;

    // Set the initial point as the last-drawn point.
    from = cur;
    // Start from the first formant pair.
    from_pair = 0;
    // Set up initial graph params.
    setupParams(&pairs[0]);

    timespec_init(&start);
    timer.start(TIMER_INTERVAL);
}

void MainWindow::setupParams(const pair_t *pair) {
    x_range = pair->x - from.x;
    slope = (pair->y - from.y) / x_range;
}

void MainWindow::plotFormant() {
    timespec_t now;
    uintmax_t diff;
    size_t pair;
    const pair_t *to;

    timespec_init(&now);
    diff = timespec_diff(&start, &now);
    pair = diff / nsec_per_pair;

    // Just leave the tracers at the last position if there are no more pairs.
    if (pair >= pair_count)
        return;

    to = &pairs[pair];

    if (pair != from_pair) {
        from = pairs[from_pair];
        from_pair = pair;
        setupParams(to);
    }

    cur.x = from.x + x_range * (diff - nsec_per_pair * pair) / nsec_per_pair;
    cur.y = from.y + slope * (cur.x - from.x);

    updateTracers(cur.x, cur.y);
    updateFPS();
}

void MainWindow::updateFPS() {
  #if QT_VERSION < QT_VERSION_CHECK(4, 7, 0)
    double secs = 0;
  #else
    double secs = QDateTime::currentDateTime().toMSecsSinceEpoch()/1000.0;
  #endif

  double key = secs;
  static double lastFpsKey;
  static int frameCount;
  ++frameCount;
  if (key-lastFpsKey > 2) // average fps over 2 seconds
  {
    ui->statusBar->showMessage(
          QString("%1 FPS, Total Data points: %2")
          .arg(frameCount/(key-lastFpsKey), 0, 'f', 0)
          .arg(ui->customPlot->graph(0)->data()->count())
          , 0);
    lastFpsKey = key;
    frameCount = 0;
  }
}

void MainWindow::updateTracers(formant_sample_t f2, formant_sample_t f1) {
  graph->removeData(tracers[0]->graphKey());
  graph->addData(f2, f1);

  for (size_t i = 0; i < Tracer::LAST; i += 1)
    tracers[i]->setGraphKey(tracers[i + 1]->graphKey());

  tracers[Tracer::LAST]->setGraphKey(f2);
  plot->replot();
}

void MainWindow::stop() {
    timer.stop();
}

MainWindow::~MainWindow() {
  delete ui;

  for (size_t i = 0; i < Tracer::COUNT; i += 1)
    delete tracers[i];

  free(pairs);
}
