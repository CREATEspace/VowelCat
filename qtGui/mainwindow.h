// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <inttypes.h>
#include <pthread.h>
#include <stddef.h>
#include <time.h>

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>

extern "C" {
    #include "audio.h"
    #include "formant.h"
}

#include "plotter.h"
#include "qcustomplot.h"
#include "timespec.h"
#include "ui_mainwindow.h"

class Plotter;

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

public:
    Plotter *plotter;

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

    QPushButton *axisButton;
    QPushButton *resetButton;
    QPushButton *chineseButton;
    QPushButton *defaultSymbolsButton;
    QPushButton *addSymbolButton;
    QPlainTextEdit *plainTextEdit;
    QGroupBox *englishGroupBox;
    QGroupBox *chineseGroupBox;
    bool vowelToggle;
    bool symbolToggle;

    QVector<QCPItemText*> vowelSymbols;
    QVector<QPushButton*> vowelButtons;

    enum {
        DEFAULT     = 0,
        INVERT_AXES = 1 << 0,
    };
    uint32_t flags;

    enum {
        LISTENING  = 0,
        PLAYING    = 1 << 0,
        RECORDING  = 1 << 1,
        PAUSING    = 1 << 2
    };
    uint32_t mflags;

    audio_t *audio;

public:
    explicit MainWindow(audio_t *a);
    ~MainWindow();

    void plotFormant(formant_sample_t f1, formant_sample_t f2, uintmax_t dur_);


private slots:
    void plotNext();

    void vowelButtonPushed(int vowelButtonPushed);
    void resetButtonPushed();
    void chineseButtonPushed();
    void defaultSymbolsButtonPushed();
    void addSymbolButtonPushed();

    void invertAxes();

    void newAudio();
    void startRecord();
    void stopAudio();

    void startPlay();
    void pauseAudio();
    void beginAudio();
    void endAudio();


    void mouseMove(QMouseEvent*);
    void mouseRelease();

private:
    void setupPlot();
    void setupEnglishButtons();
    void setupEnglishSymbols();
    void setupChineseButtons();
    void setupChineseSymbols();
    void addSymbol(QString symbol);

    void updateTracers(formant_sample_t x, formant_sample_t y);
    void pauseTracers(size_t offset);
    void clearTracer();

    void updateFPS() const;
    void updateButtons();
};

#endif // MAINWINDOW_H
