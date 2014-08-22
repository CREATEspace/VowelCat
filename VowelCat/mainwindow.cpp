// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <iostream>

#include <QColor>
#include <QFile>
#include <QFileDialog>
#include <QObject>
#include <QSignalMapper>
#include <QWidget>

extern "C" {
#include "audio.h"
#include "formant.h"
}

#include "mainwindow.h"
#include "plotter.h"
#include "spectrogram.h"
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
    QCPItemTracer(plot),
    brush(QColor::fromRgba(rgba(i)))
{
    setGraph(graph);
    setStyle(QCPItemTracer::tsCircle);
    setPen(Qt::NoPen);
    setSize(size(i));

    setSelectable(false);
    setSelectedBrush(brush);
    setSelectedPen(Qt::NoPen);
}

void Tracer::show() {
    setBrush(brush);
}

void Tracer::hide() {
    setBrush(Qt::NoBrush);
}

MainWindow::MainWindow(audio_t *a, Formants *f, Plotter *p, Spectrogram *s):
    ui(new Ui::MainWindow),
    tracer(Tracer::COUNT),
    plot_lock(PTHREAD_MUTEX_INITIALIZER),
    curChart(0),
    flags(DEFAULT),
    audio(a),
    formants(f),
    plotter(p)
{
    // Start at the origin for lack of a better place.
    cur = (pair_t) {
        .x = 0,
        .y = 0,
    };

    ui->setupUi(this);
    ui->specContainer->addWidget(s);

    QIcon::setThemeName("tango");
    ui->beginButton->setIcon(QIcon::fromTheme("media-skip-backward"));
    ui->recordButton->setIcon(QIcon::fromTheme("media-record"));
    ui->stopButton->setIcon(QIcon::fromTheme("media-playback-stop"));
    ui->playButton->setIcon(QIcon::fromTheme("media-playback-start"));
    ui->pauseButton->setIcon(QIcon::fromTheme("media-playback-pause"));
    ui->endButton->setIcon(QIcon::fromTheme("media-skip-forward"));

    ui->englishGroupBox->setStyleSheet("QPushButton {font-size:18pt;}");

    ui->label->setAlignment(Qt::AlignRight);

    plot = ui->customPlot;
    graph = plot->addGraph();

    connect(&timer, SIGNAL(timeout()), this, SLOT(plotNext()));

    connect(plot, SIGNAL(mouseMove(QMouseEvent*)), this, SLOT(mouseMove(QMouseEvent*)));
    connect(plot, SIGNAL(mouseRelease(QMouseEvent*)), this, SLOT(mouseRelease()));

    connect(ui->toggleChart, SIGNAL(released()), this, SLOT(toggleChart()));
    connect(ui->addSymbolButton, SIGNAL(released()), this, SLOT(addSymbolButtonPushed()));

    connect(ui->actionOpen, SIGNAL(triggered()), this, SLOT(openFile())); //Open audio
    connect(ui->actionSaveAs, SIGNAL(triggered()), this, SLOT(saveAsFile())); //SaveAs audio
    connect(ui->actionOpenSymbols, SIGNAL(triggered()), this, SLOT(loadSymbols()));
    connect(ui->actionSaveSymbols, SIGNAL(triggered()), this, SLOT(saveSymbols()));
    connect(ui->actionQuit, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(ui->actionInvertAxis, SIGNAL(triggered()), this, SLOT(invertAxis()));

    connect(ui->playButton, SIGNAL(clicked()), this, SLOT(startPlay())); //Play
    connect(ui->recordButton, SIGNAL(clicked()), this, SLOT(startRecord())); //Record
    connect(ui->pauseButton, SIGNAL(clicked()), this, SLOT(pauseAudio())); //Pause
    connect(ui->stopButton, SIGNAL(clicked()), this, SLOT(stopAudio())); //Stop
    connect(ui->beginButton, SIGNAL(clicked()), this, SLOT(beginAudio())); //Skip to Begin
    connect(ui->endButton, SIGNAL(clicked()), this, SLOT(endAudio())); //Skip to End
    connect(ui->actionNew, SIGNAL(triggered()), this, SLOT(newAudio())); //Clear audio

    timer.start(TIMER_INTERVAL);

    ui->actionSaveAs->setEnabled(false);

    vowelButtons.resize(36);
    setupVowelButtons();

    initButtons();
    setupPlot();
}

void MainWindow::openFile() {
    timer.stop();
    plotter->stop();
    audio_clear(audio);

    FILE *fp;
    QString qfilename;
    QByteArray qunicode;
    const char *filename;

    qfilename = QFileDialog::getOpenFileName(this, tr("Open Audio File"), "", tr("Audio-Files(*.raw *.wav)"));

    if (qfilename == NULL)
        return;

    qunicode = qfilename.toUtf8();
    filename = qunicode.constData();
    fp = fopen(filename, "r");
    audio_open(audio, fp);
    fclose(fp);

    ui->recordButton->setVisible(false);
    ui->stopButton->setVisible(true);
    ui->playButton->setVisible(true);
    ui->pauseButton->setVisible(false);
    ui->stopButton->setEnabled(false);
    ui->beginButton->setEnabled(true);
    ui->playButton->setEnabled(true);
    ui->endButton->setEnabled(true);
    ui->actionSaveAs->setEnabled(false);

    emit clearAudio();
}

void MainWindow::saveAsFile() {
    FILE *fp;
    QString qfilename;
    QByteArray qunicode;
    const char *filename;

    qfilename = QFileDialog::getSaveFileName(this, tr("Save Audio File"), "", tr("Audio-Files(*.wav)"));
    qunicode = qfilename.toUtf8();
    filename = qunicode.constData();

    fp = fopen(filename, "wb");
    audio_save(audio, fp);
    fclose(fp);

    ui->actionSaveAs->setEnabled(false);
}

void MainWindow::initButtons() {
    ui->recordButton->setVisible(true);
    ui->stopButton->setVisible(false);
    ui->playButton->setVisible(true);
    ui->pauseButton->setVisible(false);

    ui->recordButton->setEnabled(true);
    ui->beginButton->setEnabled(false);
    ui->playButton->setEnabled(false);
    ui->endButton->setEnabled(false);
}

void MainWindow::seekAudio(size_t offset) {
    audio_seek(audio, offset);
}

void MainWindow::newAudio() {
    timer.start(TIMER_INTERVAL);

    initButtons();

    plotter->stop();
    audio_clear(audio);
    emit clearAudio();
    plotter->listen();
}

void MainWindow::startRecord() {
    timer.start(TIMER_INTERVAL);

    ui->recordButton->setVisible(false);
    ui->stopButton->setVisible(true);
    ui->stopButton->setEnabled(true);

    plotter->stop();
    audio_reset(audio);
    plotter->record();
}

void MainWindow::stopAudio() {
    ui->stopButton->setEnabled(false);
    ui->beginButton->setEnabled(false);
    ui->playButton->setEnabled(true);
    ui->endButton->setEnabled(true);

    ui->actionSaveAs->setEnabled(true);

    plotter->stop();
    emit audioSeek(0);
    pauseTracers(0);
}

void MainWindow::startPlay() {
    timer.start(TIMER_INTERVAL);

    ui->pauseButton->setVisible(true);
    ui->playButton->setVisible(false);
    ui->beginButton->setEnabled(false);
    ui->endButton->setEnabled(false);

    plotter->stop();
    audio_reset(audio);
    plotter->play();
}

void MainWindow::pauseAudio() {
    ui->pauseButton->setVisible(false);
    ui->playButton->setVisible(true);
    ui->playButton->setEnabled(audio->prbuf_offset != audio->prbuf_size);
    ui->beginButton->setEnabled(true);
    ui->endButton->setEnabled(audio->prbuf_offset != audio->prbuf_size);

    plotter->stop();

    pauseTracers(audio->prbuf_offset - audio->samples_per_chunk);
}

void MainWindow::beginAudio() {
    ui->playButton->setEnabled(true);
    ui->beginButton->setEnabled(false);
    ui->endButton->setEnabled(true);

    plotter->stop();
    emit audioSeek(0);
    pauseTracers(0);
}

void MainWindow::endAudio() {
    ui->playButton->setEnabled(false);
    ui->beginButton->setEnabled(true);
    ui->endButton->setEnabled(false);

    plotter->stop();
    emit audioSeek(audio->prbuf_size);
    pauseTracers(audio->prbuf_size - audio->samples_per_chunk);
}

void MainWindow::loadSymbols() {
    auto qfilename = QFileDialog::getOpenFileName(this, "Open Vowel Symbols");
    auto qunicode = qfilename.toUtf8();
    auto path = qunicode.constData();

    FILE *stream = fopen(path, "r");

    if (!stream)
        // TODO: proper error handling.
        return;

    PhoneticChart pc(ui->customPlot, ui->label);

    if (!pc.load(stream)) {
        fclose(stream);
        return;
    }

    if (charts.size())
        charts[curChart].uninstall();

    curChart = charts.size();
    charts.push_back(pc);
    pc.install();

    plot->replot();

    fclose(stream);
}

void MainWindow::saveSymbols() {
    auto qfilename = QFileDialog::getSaveFileName(this, "Save Vowel Symbols");
    auto qunicode = qfilename.toUtf8();
    auto path = qunicode.constData();

    FILE *stream = fopen(path, "w");

    if (!stream)
        // TODO: proper error handling.
        return;

    if (charts.size())
        charts[curChart].save(stream);

    fclose(stream);
}

void MainWindow::setupPlot()
{
    plot->setInteractions(QCP::iRangeZoom | QCP::iSelectItems);
    graph->setPen(Qt::NoPen);

    plot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    plot->xAxis->setRange(2400, 680);
    plot->xAxis->setRangeReversed(true);
    plot->xAxis->setLabel("F2 (Hz)");

    plot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
    plot->yAxis->setRange(900, 150);
    plot->yAxis->setRangeReversed(true);
    plot->yAxis->setLabel("F1 (Hz)");

    loadCharts();
    setupVowelBox();

    for (size_t i = 0; i < Tracer::COUNT; i += 1)
        tracers[i] = new Tracer(plot, graph, i);

    hideTracers();
}

void MainWindow::loadCharts() {
    QStringList nameFilter("*.sym");
    QDir directory(QString("%1/charts").arg(QDir::currentPath()));
    QStringList symbolFiles = directory.entryList(nameFilter);
    QStringList::const_iterator it;

    for (it = symbolFiles.constBegin(); it != symbolFiles.constEnd(); ++it){
        auto utf8 = (*it).toUtf8();
        FILE *stream = fopen(utf8.constData(), "r");

        if (!stream)
            continue;

        PhoneticChart chart(ui->customPlot, ui->label);
        chart.load(stream);
        chart.hide();

        charts.push_back(chart);
    }

    if (!charts.size())
        return;

    curChart = charts.size() - 1;
    charts[curChart].install();
}

void MainWindow::setupVowelBox() {
    // h is for horizontal, from the bottommost line to the top line
    // of the IPA chart (4 lines)
    QCPItemLine *h0Line = new QCPItemLine(plot);
    h0Line->start->setCoords(800, 793); // ɑ
    h0Line->end->setCoords(1632, 793); // a

    QCPItemLine *h1Line = new QCPItemLine(plot);
    h1Line->start->setCoords(800, 541); // ɔ
    h1Line->end->setCoords(1998, 541); // ɛ

    QCPItemLine *h2Line = new QCPItemLine(plot);
    h2Line->start->setCoords(2148, 420); // e
    h2Line->end->setCoords(800, 420); // o

    QCPItemLine *h3Line = new QCPItemLine(plot);
    h3Line->start->setCoords(800, 295); // u
    h3Line->end->setCoords(2343, 295); // i

    // v is for vertical, from left to right
    // of the IPA chart (3 lines)
    QCPItemLine *v0Line = new QCPItemLine(plot);
    v0Line->start->setCoords(2343, 295);
    v0Line->end->setCoords(1632, 793);

    QCPItemLine *v1Line = new QCPItemLine(plot);
    v1Line->start->setCoords(1500, 300);
    v1Line->end->setCoords(1400, 793);

    QCPItemLine *v2Line = new QCPItemLine(plot);
    v2Line->start->setCoords(800, 793);
    v2Line->end->setCoords(800, 295);

    vowelBox.resize(7);
    vowelBox[0] = h0Line;
    vowelBox[1] = h1Line;
    vowelBox[2] = h2Line;
    vowelBox[3] = h3Line;
    vowelBox[4] = v0Line;
    vowelBox[5] = v1Line;
    vowelBox[6] = v2Line;

    for (int i = 0; i < 7; i++){
        plot->addItem(vowelBox[i]);
        vowelBox[i]->setSelectable(false);
    }
}

void MainWindow::showTracers() {
    for (size_t i = 0; i < Tracer::COUNT; i += 1)
        tracers[i]->show();
}

void MainWindow::hideTracers() {
    for (size_t i = 0; i < Tracer::COUNT; i += 1)
        tracers[i]->hide();
}

void MainWindow::setupVowelButtons() {
    vowelButtons[0] = ui->pushButton;
    vowelButtons[1] = ui->pushButton_2;
    vowelButtons[2] = ui->pushButton_3;
    vowelButtons[3] = ui->pushButton_4;
    vowelButtons[4] = ui->pushButton_5;
    vowelButtons[5] = ui->pushButton_6;
    vowelButtons[6] = ui->pushButton_7;
    vowelButtons[7] = ui->pushButton_8;
    vowelButtons[8] = ui->pushButton_9;
    vowelButtons[9] = ui->pushButton_10;
    vowelButtons[10] = ui->pushButton_11;
    vowelButtons[11] = ui->pushButton_12;
    vowelButtons[12] = ui->pushButton_13;
    vowelButtons[13] = ui->pushButton_14;
    vowelButtons[14] = ui->pushButton_15;
    vowelButtons[15] = ui->pushButton_16;
    vowelButtons[16] = ui->pushButton_17;
    vowelButtons[17] = ui->pushButton_18;
    vowelButtons[18] = ui->pushButton_19;
    vowelButtons[19] = ui->pushButton_20;
    vowelButtons[20] = ui->pushButton_21;
    vowelButtons[21] = ui->pushButton_22;
    vowelButtons[22] = ui->pushButton_23;
    vowelButtons[23] = ui->pushButton_24;
    vowelButtons[24] = ui->pushButton_25;
    vowelButtons[25] = ui->pushButton_26;
    vowelButtons[26] = ui->pushButton_27;
    vowelButtons[27] = ui->pushButton_28;
    vowelButtons[28] = ui->pushButton_29;
    vowelButtons[29] = ui->pushButton_30;
    vowelButtons[30] = ui->pushButton_31;
    vowelButtons[31] = ui->pushButton_32;
    vowelButtons[32] = ui->pushButton_33;
    vowelButtons[33] = ui->pushButton_34;
    vowelButtons[34] = ui->pushButton_35;
    vowelButtons[35] = ui->pushButton_36;

    // Connect every button to a single function using a QSignalMapper
    // Rather than define unique functions for every button, we use the
    // mapper to pass in an integer argument that refers to the pushed
    // button's place in the vowelButtons array.
    for (int i = 0; i < vowelButtons.size(); i++){
        connect(vowelButtons[i], SIGNAL(released()), &signalMapper, SLOT(map()));
        signalMapper.setMapping(vowelButtons[i], i);
    }

    connect(&signalMapper, SIGNAL(mapped(int)), this, SLOT(vowelButtonPushed(int)));

    // Disable portions of the chart that have no vowel symbol
    vowelButtons[8]->setEnabled(false);
    vowelButtons[9]->setEnabled(false);
    vowelButtons[10]->setEnabled(false);
    vowelButtons[25]->setEnabled(false);
    vowelButtons[27]->setEnabled(false);
    vowelButtons[28]->setEnabled(false);
    vowelButtons[29]->setEnabled(false);
    vowelButtons[33]->setEnabled(false);

    // Add tooltips for vowel symbols
    // Code blocks are split up into rows of vowels
    vowelButtons[0]->setToolTip("close front unrounded");
    vowelButtons[1]->setToolTip("close front rounded");
    vowelButtons[2]->setToolTip("close central unrounded");
    vowelButtons[3]->setToolTip("close central rounded");
    vowelButtons[4]->setToolTip("close back unrounded");
    vowelButtons[5]->setToolTip("close back rounded");

    vowelButtons[6]->setToolTip("near-close near-front unrounded");
    vowelButtons[7]->setToolTip("near-close near-front rounded");
    vowelButtons[11]->setToolTip("near-close near-back rounded");

    vowelButtons[12]->setToolTip("close-mid front unrounded");
    vowelButtons[13]->setToolTip("close-mid front rounded");
    vowelButtons[14]->setToolTip("close-mid central unrounded");
    vowelButtons[15]->setToolTip("close-mid central rounded");
    vowelButtons[16]->setToolTip("close-mid back unrounded");
    vowelButtons[17]->setToolTip("close-mid back rounded");

    vowelButtons[18]->setToolTip("open-mid front unrounded");
    vowelButtons[19]->setToolTip("open-mid front rounded");
    vowelButtons[20]->setToolTip("open-mid central unrounded");
    vowelButtons[21]->setToolTip("open-mid central rounded");
    vowelButtons[22]->setToolTip("open-mid back unrounded");
    vowelButtons[23]->setToolTip("open-mid back rounded");

    vowelButtons[24]->setToolTip("near-open front unrounded");
    vowelButtons[26]->setToolTip("near-open central unrounded");

    vowelButtons[30]->setToolTip("open front unrounded");
    vowelButtons[31]->setToolTip("open front rounded");
    vowelButtons[32]->setToolTip("open central unrounded");
    vowelButtons[34]->setToolTip("open back unrounded");
    vowelButtons[35]->setToolTip("open back rounded");
}

void MainWindow::plotFormant(formant_sample_t f1, formant_sample_t f2) {
    pthread_mutex_lock(&plot_lock);

    // Copy the raw formants into our pairs structure.
    to = (pair_t) {
        .x = f2,
        .y = f1,
    };

    // Set the last-drawn point as the initial point.
    from = cur;

    x_range = to.x - from.x;
    slope = (to.y - from.y) / x_range;

    tracer = 0;
    showTracers();

    timespec_init(&start);

    pthread_mutex_unlock(&plot_lock);
}

void MainWindow::plotNext() {
    timespec_t now;
    uintmax_t diff;

    pthread_mutex_lock(&plot_lock);

    timespec_init(&now);
    diff = timespec_diff(&start, &now);

    if (diff > DUR) {
        timer.setInterval(TIMER_SLOWDOWN);

        if (tracer == Tracer::COUNT) {
            pthread_mutex_unlock(&plot_lock);
            return;
        }

        clearTracer();
        tracer += 1;

        pthread_mutex_unlock(&plot_lock);

        return;
    }

    cur.x = from.x + x_range * diff / DUR;
    cur.y = from.y + (formant_sample_t)(slope * (cur.x - from.x));

    pthread_mutex_unlock(&plot_lock);

    timer.setInterval(TIMER_INTERVAL);

    updateTracers(cur.x, cur.y);
    updateFPS();
}

void MainWindow::updateFPS() const {
#ifdef LOG_FPS
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
    printf("%f FPS", frameCount/(key-lastFpsKey));
    lastFpsKey = key;
    frameCount = 0;
  }
#endif
}

void MainWindow::updateTracers(formant_sample_t f2, formant_sample_t f1) {
    for (size_t i = 0; i < Tracer::LAST; i += 1)
        tracers[i]->position->setCoords(tracers[i + 1]->position->coords());

    tracers[Tracer::LAST]->position->setCoords(f2, f1);
    plot->replot();
}

void MainWindow::pauseTracers(size_t offset) {
    if (offset >= audio->prbuf_size)
        return;

    timer.stop();
    hideTracers();

    if (!formants->calc(offset)) {
        plot->replot();
        return;
    }

    tracers[Tracer::LAST]->show();
    tracers[Tracer::LAST]->position->setCoords(formants->f2, formants->f1);

    plot->replot();
}

void MainWindow::clearTracer() {
    for (size_t i = 0; i < Tracer::LAST; i += 1)
        tracers[i]->position->setCoords(tracers[i + 1]->position->coords());

    tracers[Tracer::LAST - tracer]->hide();
    plot->replot();
}

void MainWindow::spectroClicked(size_t offset) {
    if (offset > audio->prbuf_size)
        return;

    emit audioSeek(offset);

    ui->playButton->setEnabled(audio->prbuf_offset != audio->prbuf_size);
    ui->endButton->setEnabled(audio->prbuf_offset != audio->prbuf_size);
    ui->beginButton->setEnabled(audio->prbuf_offset != 0);

    pauseTracers(offset);
}

// When a vowel Button is pushed, we want to create a new
// item text, set its attributes, add it to the vector
// of vowelSymbols and the plot, and replot everything
void MainWindow::vowelButtonPushed(int pushedVowelButton){
    addSymbol(vowelButtons[pushedVowelButton]->text());
}

void MainWindow::toggleChart(){
    if (charts.size() == 0) return;
    charts[curChart].uninstall();
    curChart += 1;
    curChart %= charts.size();
    charts[curChart].install();
    plot->replot();
}

void MainWindow::addSymbolButtonPushed(){

    if (ui->plainTextEdit->toPlainText().length() > 2)
    {
        QString text = ui->plainTextEdit->toPlainText();
        text.chop(text.length() - 2); // Cut off at 300 characters
        ui->plainTextEdit->setPlainText(text); // Reset text

        // This code just resets the cursor back to the end position
        // If you don't use this, it moves back to the beginning.
        // This is helpful for really long text edits where you might
        // lose your place.
        QTextCursor cursor = ui->plainTextEdit->textCursor();
        cursor.setPosition(ui->plainTextEdit->document()->characterCount() - 1);
        ui->plainTextEdit->setTextCursor(cursor);

        // This is your "action" to alert the user. I'd suggest something more
        // subtle though, or just not doing anything at all.
        QMessageBox::critical(this,
                              "Error",
                              "Your symbol can only have 2 characters :(");
    } else {
        addSymbol(ui->plainTextEdit->toPlainText());
    }
}

void MainWindow::addSymbol(QString symbol){
    if (charts.size() > 0) charts[curChart].addSymbol(symbol, 500, 1500);
    plot->replot();
}

void MainWindow::invertAxis(){
    flags ^= INVERT_AXIS;
    plot->yAxis->setRangeReversed(!(flags & INVERT_AXIS));
    plot->replot();
}

void MainWindow::mouseMove(QMouseEvent *event){
    if (charts.size() > 0) charts[curChart].mouseMove(event);
}

void MainWindow::mouseRelease(){
    QList<QCPAbstractItem*> selected = plot->selectedItems();
    for (int i = 0; i < selected.size(); i++){
        selected[i]->setSelected(false);
    }
    plot->replot();
}

MainWindow::~MainWindow() {
    delete ui;

    for (size_t i = 0; i < Tracer::COUNT; i += 1)
        delete tracers[i];

    // TODO: fix memory leaks.
}
