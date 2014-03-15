#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include <QColor>
#include <QObject>
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
    ui(new Ui::MainWindow),
    tracer(Tracer::COUNT),
    plot_lock(PTHREAD_MUTEX_INITIALIZER)
{


    // Start at the origin for lack of a better place.
    cur = (pair_t) {
        .x = 0,
        .y = 0,
    };

    ui->setupUi(this);
    setGeometry(400, 250, 1000, 800);

    reversed = true;
    
    axisButton = ui->axisButton;
    plot = ui->customPlot;
    graph = plot->addGraph();

    connect(axisButton, SIGNAL(released()), this, SLOT(axisButtonPushed()));
    QObject::connect(&timer, &QTimer::timeout,
                     this, &MainWindow::plotNext);
    timer.start(TIMER_INTERVAL);

    setupPlot();
}

void MainWindow::setupPlot()
{
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectItems);
    graph->setPen(Qt::NoPen);
    graph->addData(DBL_MAX, DBL_MAX);

    plot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    plot->xAxis->setRange(2400, 700);
    plot->xAxis->setRangeReversed(true);
    plot->xAxis->setLabel("F2 (Hz)");

    plot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
    plot->yAxis->setRange(900, 250);
    plot->yAxis->setRangeReversed(true);
    plot->yAxis->setLabel("F1 (Hz)");

    QVector<QCPItemText*> vowelSymbols(13);

    QCPItemText *upperHighBackRounded = new QCPItemText(ui->customPlot);
    upperHighBackRounded->position->setCoords(750, 295);
    upperHighBackRounded->setText("u");

    QCPItemText *lowerHighBackRounded = new QCPItemText(ui->customPlot);
    lowerHighBackRounded->position->setCoords(910, 334);
    lowerHighBackRounded->setText("ʊ");

    QCPItemText *upperMidBackRounded = new QCPItemText(ui->customPlot);
    upperMidBackRounded->position->setCoords(727, 406);
    upperMidBackRounded->setText("o");

    QCPItemText *lowerMidBackRounded = new QCPItemText(ui->customPlot);
    lowerMidBackRounded->position->setCoords(830, 541);
    lowerMidBackRounded->setText("ɔ");

    QCPItemText *lowerLowBackRounded = new QCPItemText(ui->customPlot);
    lowerLowBackRounded->position->setCoords(843, 652);
    lowerLowBackRounded->setText("ɒ");

    QCPItemText *lowerLowBackUnrounded = new QCPItemText(ui->customPlot);
    lowerLowBackUnrounded->position->setCoords(1065, 781);
    lowerLowBackUnrounded->setText("ɑ");

    QCPItemText *lowerLowCentralUnrounded = new QCPItemText(ui->customPlot);
    lowerLowCentralUnrounded->position->setCoords(1211, 784);
    lowerLowCentralUnrounded->setText("ä");

    QCPItemText *lowerLowFrontUnrounded = new QCPItemText(ui->customPlot);
    lowerLowFrontUnrounded->position->setCoords(1632, 806);
    lowerLowFrontUnrounded->setText("a");

    QCPItemText *upperLowFrontUnrounded = new QCPItemText(ui->customPlot);
    upperLowFrontUnrounded->position->setCoords(1782, 766);
    upperLowFrontUnrounded->setText("æ");

    QCPItemText *lowerMidFrontUnrounded = new QCPItemText(ui->customPlot);
    lowerMidFrontUnrounded->position->setCoords(1840, 541);
    lowerMidFrontUnrounded->setText("ɛ");

    QCPItemText *upperMidFrontUnrounded = new QCPItemText(ui->customPlot);
    upperMidFrontUnrounded->position->setCoords(2148, 434);
    upperMidFrontUnrounded->setText("e");

    QCPItemText *lowerHighFrontUnrounded = new QCPItemText(ui->customPlot);
    lowerHighFrontUnrounded->position->setCoords(2187, 360);
    lowerHighFrontUnrounded->setText("ɪ");

    QCPItemText *upperHighFrontUnrounded = new QCPItemText(ui->customPlot);
    upperHighFrontUnrounded->position->setCoords(2343, 294);
    upperHighFrontUnrounded->setText("i");

    vowelSymbols[0] = upperHighBackRounded;
    vowelSymbols[1] = lowerHighBackRounded;
    vowelSymbols[2] = upperMidBackRounded;
    vowelSymbols[3] = lowerMidBackRounded;
    vowelSymbols[4] = lowerLowBackRounded;
    vowelSymbols[5] = lowerLowBackUnrounded;
    vowelSymbols[6] = lowerLowCentralUnrounded;
    vowelSymbols[7] = lowerLowFrontUnrounded;
    vowelSymbols[8] = upperLowFrontUnrounded;
    vowelSymbols[9] = lowerMidFrontUnrounded;
    vowelSymbols[10] = upperMidFrontUnrounded;
    vowelSymbols[11] = lowerHighFrontUnrounded;
    vowelSymbols[12] = upperHighFrontUnrounded;

    for (int i = 0; i < 13; i++){
        ui->customPlot->addItem(vowelSymbols[i]);
        vowelSymbols[i]->setFont(QFont(font().family(), 30));
        vowelSymbols[i]->setSelectedColor(Qt::blue);
        vowelSymbols[i]->setSelectedFont(QFont(font().family(), 30));
    }

    for (size_t i = 0; i < Tracer::COUNT; i += 1)
        tracers[i] = new Tracer(plot, graph, i);
}

void MainWindow::plotFormant(formant_sample_t f1, formant_sample_t f2,
                             uintmax_t dur)
{
    pthread_mutex_lock(&plot_lock);

    // Copy the raw formants into our pairs structure.
    to = (pair_t) {
        .x = f2,
        .y = f1,
    };

    this->dur = dur;

    // Set the last-drawn point as the initial point.
    from = cur;

    x_range = to.x - from.x;
    slope = (to.y - from.y) / x_range;

    tracer = 0;
    timespec_init(&start);

    pthread_mutex_unlock(&plot_lock);
}

void MainWindow::plotNext() {
    timespec_t now;
    uintmax_t diff;

    pthread_mutex_lock(&plot_lock);

    timespec_init(&now);
    diff = timespec_diff(&start, &now);

    if (diff > dur) {
        timer.setInterval(50);

        if (tracer == Tracer::COUNT) {
            pthread_mutex_unlock(&plot_lock);
            return;
        }

        clearTracer();
        tracer += 1;

        pthread_mutex_unlock(&plot_lock);

        return;
    }

    cur.x = from.x + x_range * diff / dur;
    cur.y = from.y + slope * (cur.x - from.x);

    pthread_mutex_unlock(&plot_lock);

    timer.setInterval(TIMER_INTERVAL);

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
    if (tracers[0]->graphKey() != DBL_MAX)
        graph->removeData(tracers[0]->graphKey());

    graph->addData(f2, f1);

    for (size_t i = 0; i < Tracer::LAST; i += 1)
        tracers[i]->setGraphKey(tracers[i + 1]->graphKey());

    tracers[Tracer::LAST]->setGraphKey(f2);
    plot->replot();
}

void MainWindow::clearTracer() {
    if (tracers[0]->graphKey() != DBL_MAX)
        graph->removeData(tracers[0]->graphKey());

    for (size_t i = 0; i < Tracer::LAST; i += 1)
        tracers[i]->setGraphKey(tracers[i + 1]->graphKey());

    if (tracers[Tracer::LAST - tracer]->graphKey() != DBL_MAX)
        graph->removeData(tracers[Tracer::LAST - tracer]->graphKey());

    tracers[Tracer::LAST - tracer]->setGraphKey(DBL_MAX);
    plot->replot();
}

void MainWindow::axisButtonPushed(){
    reversed = !reversed;
    plot->yAxis->setRangeReversed(reversed);
    plot->replot();
}

MainWindow::~MainWindow() {
    delete ui;

    for (size_t i = 0; i < Tracer::COUNT; i += 1)
        delete tracers[i];
}