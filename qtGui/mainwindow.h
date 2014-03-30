#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <inttypes.h>
#include <pthread.h>
#include <stddef.h>
#include <time.h>

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>

#include "formant.h"
#include "qcustomplot.h"
#include "timespec.h"
#include "ui_mainwindow.h"

class Tracer: public QCPItemTracer {
public:
    enum { COUNT = 16 };
    enum { LAST = COUNT - 1 };

private:
    enum { DIAM_MAX = 40 };
    enum { DIAM_MIN = 14 };
    enum { DIAM_RANGE = DIAM_MAX - DIAM_MIN };

    // Red.
    enum { COLOR_BASE = 0xff0000 };

    enum { ALPHA_MAX = 255 };
    enum { ALPHA_MIN = 0 };
    enum { ALPHA_RANGE = ALPHA_MAX - ALPHA_MIN };

public:
    Tracer(QCustomPlot *plot, QCPGraph *graph, size_t i);

private:
    // Calculate a tracer diameter f(x) for the tracer x s.t.
    //
    //   f(0) = DIAM_MIN
    //   f(Tracer::LAST) = DIAM_MAX
    //
    // and all values in between follow a simple linear curve.
    static double size(double x);

    // Calculate an alpha value f(x) for the tracer x s.t.
    //
    //   f(0) = ALPHA_MIN
    //   f(Tracer::LAST) = ALPHA_MAX
    //
    // and all values inbetween follow an exponential curve.
    static uint32_t alpha(double x);

    // Splice together an RGBA integer for QColor::fromRgba of the form AARRGGBB.
    static uint32_t rgba(double i);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    enum { TIMER_INTERVAL = 10 };

    typedef struct {
        formant_sample_t x, y;
    } pair_t;

    pair_t from, to;
    pair_t cur;

    uintmax_t dur;
    timespec_t start;

    double slope;
    double x_range;

    Ui::MainWindow *ui;
    QCustomPlot *plot;
    QCPGraph *graph;

    Tracer *tracers[Tracer::COUNT];
    size_t tracer;

    pthread_mutex_t plot_lock;
    QTimer timer;

    QPushButton *aButton;
    bool reversed;

public:
    explicit MainWindow(QWidget *parent = NULL);
    ~MainWindow();

    void plotFormant(formant_sample_t f1, formant_sample_t f2, uintmax_t dur);

private slots:
    void plotNext();
    void axesButtonPushed();

private:
    void setupPlot();

    void updateTracers(formant_sample_t x, formant_sample_t y);
    void clearTracer();

    void updateFPS();
};

#endif // MAINWINDOW_H
