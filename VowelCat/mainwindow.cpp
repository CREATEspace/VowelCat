// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

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

class VowelSymbol: public QCPItemText {
private:
    enum { FONT_SIZE = 30 };

public:
    VowelSymbol(QCustomPlot *plot, wchar_t symbol, uint32_t f1,
                uint32_t f2):
        QCPItemText(plot)
    {
        QFont font(plot->font().family(), FONT_SIZE);

        position->setCoords(f2, f1);
        setFont(font);
        setSelectedFont(font);
        setSelectedColor(Qt::blue);
        setText(QString::fromWCharArray(&symbol, 1));
    }
};

MainWindow::MainWindow(audio_t *a, Formants *f, Plotter *p, Spectrogram *s):
    ui(new Ui::MainWindow),
    tracer(Tracer::COUNT),
    plot_lock(PTHREAD_MUTEX_INITIALIZER),
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
    ui->chineseGroupBox->setStyleSheet("QPushButton {font-size:18pt;}");

    ui->chineseGroupBox->setVisible(false);
    ui->label->setAlignment(Qt::AlignRight);
    accentToggle = 0;
    vowelToggle = true;

    plot = ui->customPlot;
    graph = plot->addGraph();

    connect(&timer, SIGNAL(timeout()), this, SLOT(plotNext()));

    connect(plot, SIGNAL(mouseMove(QMouseEvent*)), this, SLOT(mouseMove(QMouseEvent*)));
    connect(plot, SIGNAL(mouseRelease(QMouseEvent*)), this, SLOT(mouseRelease()));

    connect(ui->defaultSymbolsButton, SIGNAL(released()), this, SLOT(defaultSymbolsButtonPushed()));
    connect(ui->addSymbolButton, SIGNAL(released()), this, SLOT(addSymbolButtonPushed()));
    connect(ui->accentToggleButton, SIGNAL(released()), this, SLOT(accentButtonPushed()));

    connect(ui->actionOpen, SIGNAL(triggered()), this, SLOT(openFile())); //Open audio
    connect(ui->actionSaveAs, SIGNAL(triggered()), this, SLOT(saveAsFile())); //SaveAs audio
    connect(ui->actionOpenSymbols, SIGNAL(triggered()), this, SLOT(loadSymbols()));
    connect(ui->actionSaveSymbols, SIGNAL(triggered()), this, SLOT(saveSymbols()));
    connect(ui->actionQuit, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(ui->actionResetPlot, SIGNAL(triggered()), this, SLOT(resetPlot()));
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

    vowelButtons.resize(45);
    setupVowelButtons();
    setupEnglishButtons();
    setupChineseButtons();

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

    clearSymbols();
    vowelSymbols.clear();

    loadSymbols(stream);
    plot->replot();

    fclose(stream);
}

void MainWindow::loadSymbols(FILE *stream) {
    uint32_t f1, f2;
    wchar_t symbol;

    while (fscanf(stream, "%lc %u %u\n", &symbol, &f1, &f2) == 3) {
        auto vs = new VowelSymbol(plot, symbol, f1, f2);
        plot->addItem(vs);
        vowelSymbols.push_back(vs);
    }
}

void MainWindow::saveSymbols() {
    auto qfilename = QFileDialog::getSaveFileName(this, "Save Vowel Symbols");
	auto qunicode = qfilename.toUtf8();
	auto path = qunicode.constData();

    FILE *stream = fopen(path, "w");

    if (!stream)
        // TODO: proper error handling.
        return;

    saveSymbols(stream);
    fclose(stream);
}

void MainWindow::saveSymbols(FILE *stream) const {
    for (int i = 0; i < vowelSymbols.size(); i += 1) {
        auto vs = vowelSymbols[i];
        auto coords = vs->position->coords();

        fprintf(stream, "%lc\t%u\t%u\n", vs->text().at(0).unicode(),
            (uint32_t) coords.y(), (uint32_t) coords.x());
    }
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

    setupEnglishSymbols();

    for (size_t i = 0; i < Tracer::COUNT; i += 1)
        tracers[i] = new Tracer(plot, graph, i);

    hideTracers();
}

void MainWindow::showTracers() {
    for (size_t i = 0; i < Tracer::COUNT; i += 1)
        tracers[i]->show();
}

void MainWindow::hideTracers() {
    for (size_t i = 0; i < Tracer::COUNT; i += 1)
        tracers[i]->hide();
}

void MainWindow::setupChineseSymbols(){
    QCPItemText *closeFront = new QCPItemText(ui->customPlot);
    closeFront->position->setCoords(2200, 240);
    closeFront->setText("i");

    QCPItemText *closeFront2 = new QCPItemText(ui->customPlot);
    closeFront2->position->setCoords(2000, 235);
    closeFront2->setText("y");

    QCPItemText *closeBack = new QCPItemText(ui->customPlot);
    closeBack->position->setCoords(850, 300);
    closeBack->setText("u");

    QCPItemText *closeMidBack = new QCPItemText(ui->customPlot);
    closeMidBack->position->setCoords(1350, 425);
    closeMidBack->setText("ɤ");

    QCPItemText *openFront = new QCPItemText(ui->customPlot);
    openFront->position->setCoords(1400, 750);
    openFront->setText("a");

    vowelSymbols.resize(5);
    vowelSymbols[0] = closeFront;
    vowelSymbols[1] = closeFront2;
    vowelSymbols[2] = closeBack;
    vowelSymbols[3] = closeMidBack;
    vowelSymbols[4] = openFront;

    for (int i = 0; i < 5; i++){
        ui->customPlot->addItem(vowelSymbols[i]);
        vowelSymbols[i]->setFont(QFont(font().family(), 40));
        vowelSymbols[i]->setColor(QColor(34, 34, 34));
        vowelSymbols[i]->setSelectedColor(Qt::blue);
        vowelSymbols[i]->setSelectedFont(QFont(font().family(), 40));
    }

    ui->label->setText("Chinese");
}

void MainWindow::setupEnglishSymbols(){
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

    vowelSymbols.resize(13);
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
        vowelSymbols[i]->setFont(QFont(font().family(), 40));
        vowelSymbols[i]->setColor(QColor(34, 34, 34));
        vowelSymbols[i]->setSelectedColor(Qt::blue);
        vowelSymbols[i]->setSelectedFont(QFont(font().family(), 40));
    }

    ui->label->setText("International Phonetic Alphabet");

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

    vowelLines.resize(7);
    vowelLines[0] = h0Line;
    vowelLines[1] = h1Line;
    vowelLines[2] = h2Line;
    vowelLines[3] = h3Line;
    vowelLines[4] = v0Line;
    vowelLines[5] = v1Line;
    vowelLines[6] = v2Line;

    for (int i = 0; i < 7; i++){
        plot->addItem(vowelLines[i]);
        vowelLines[i]->setSelectable(false);
    }
}

void MainWindow::setupEnglishReceivedSymbols(){
    QCPItemText *one = new QCPItemText(ui->customPlot);
    one->position->setCoords(1850, 350);
    one->setText("ɪɘ");

    QCPItemText *two = new QCPItemText(ui->customPlot);
    two->position->setCoords(1100, 350);
    two->setText("ʊɘ");

    QCPItemText *three = new QCPItemText(ui->customPlot);
    three->position->setCoords(1950, 480);
    three->setText("eɪ");

    QCPItemText *four = new QCPItemText(ui->customPlot);
    four->position->setCoords(1450, 480);
    four->setText("ɘʊ");

    QCPItemText *five = new QCPItemText(ui->customPlot);
    five->position->setCoords(950, 480);
    five->setText("ɔɪ");

    QCPItemText *six = new QCPItemText(ui->customPlot);
    six->position->setCoords(1920, 560);
    six->setText("eɘ");

    QCPItemText *seven = new QCPItemText(ui->customPlot);
    seven->position->setCoords(1520, 730);
    seven->setText("aɪ");

    QCPItemText *eight = new QCPItemText(ui->customPlot);
    eight->position->setCoords(1225, 730);
    eight->setText("aʊ");

    vowelSymbols.resize(8);
    vowelSymbols[0] = one;
    vowelSymbols[1] = two;
    vowelSymbols[2] = three;
    vowelSymbols[3] = four;
    vowelSymbols[4] = five;
    vowelSymbols[5] = six;
    vowelSymbols[6] = seven;
    vowelSymbols[7] = eight;

    for (int i = 0; i < 8; i++){
        ui->customPlot->addItem(vowelSymbols[i]);
        vowelSymbols[i]->setFont(QFont(font().family(), 40));
        vowelSymbols[i]->setColor(QColor(34, 34, 34));
        vowelSymbols[i]->setSelectedColor(Qt::blue);
        vowelSymbols[i]->setSelectedFont(QFont(font().family(), 40));
    }

    QCPItemLine *oneLine = new QCPItemLine(plot);
    oneLine->start->setParentAnchor(one->position);
    oneLine->end->setCoords(1650, 450);

    QCPItemLine *twoLine = new QCPItemLine(plot);
    twoLine->start->setParentAnchor(two->position);
    twoLine->end->setCoords(1300, 440);

    QCPItemLine *threeLine = new QCPItemLine(plot);
    threeLine->start->setParentAnchor(three->position);
    threeLine->end->setCoords(1900, 400);

    QCPItemLine *fourLine = new QCPItemLine(plot);
    fourLine->start->setParentAnchor(four->position);
    fourLine->end->setCoords(1250, 400);

    QCPItemLine *fiveLine = new QCPItemLine(plot);
    fiveLine->start->setParentAnchor(five->position);
    fiveLine->end->setCoords(1550, 410);

    QCPItemLine *sixLine = new QCPItemLine(plot);
    sixLine->start->setParentAnchor(six->position);
    sixLine->end->setCoords(1700, 520);

    QCPItemLine *sevenLine = new QCPItemLine(plot);
    sevenLine->start->setParentAnchor(seven->position);
    sevenLine->end->setCoords(1650, 400);

    QCPItemLine *eightLine = new QCPItemLine(plot);
    eightLine->start->setParentAnchor(eight->position);
    eightLine->end->setCoords(1225, 425);

    vowelLines.resize(8);
    vowelLines[0] = oneLine;
    vowelLines[1] = twoLine;
    vowelLines[2] = threeLine;
    vowelLines[3] = fourLine;
    vowelLines[4] = fiveLine;
    vowelLines[5] = sixLine;
    vowelLines[6] = sevenLine;
    vowelLines[7] = eightLine;

    QPen pen;
    pen.setWidth(4);

    for (int i = 0; i < 8; i++){
        plot->addItem(vowelLines[i]);
        vowelLines[i]->setHead(QCPLineEnding::esSpikeArrow);
        vowelLines[i]->setPen(pen);
        vowelLines[i]->setSelectable(false);
    }

    ui->label->setText("English Received (British)");

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

    vowelLines.resize(15);
    vowelLines[8] = h0Line;
    vowelLines[9] = h1Line;
    vowelLines[10] = h2Line;
    vowelLines[11] = h3Line;
    vowelLines[12] = v0Line;
    vowelLines[13] = v1Line;
    vowelLines[14] = v2Line;

    for (int i = 8; i < 15; i++){
        plot->addItem(vowelLines[i]);
        vowelLines[i]->setSelectable(false);
    }
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

    vowelButtons[36] = ui->pushButton_37;
    vowelButtons[37] = ui->pushButton_38;
    vowelButtons[38] = ui->pushButton_39;
    vowelButtons[39] = ui->pushButton_40;
    vowelButtons[40] = ui->pushButton_41;
    vowelButtons[41] = ui->pushButton_42;
    vowelButtons[42] = ui->pushButton_43;
    vowelButtons[43] = ui->pushButton_44;
    vowelButtons[44] = ui->pushButton_45;

    // Connect every button to a single function using a QSignalMapper
    // Rather than define unique functions for every button, we use the
    // mapper to pass in an integer argument that refers to the pushed
    // button's place in the vowelButtons array.
    for (int i = 0; i < vowelButtons.size(); i++){
        connect(vowelButtons[i], SIGNAL(released()), &signalMapper, SLOT(map()));
        signalMapper.setMapping(vowelButtons[i], i);
    }

    connect(&signalMapper, SIGNAL(mapped(int)), this, SLOT(vowelButtonPushed(int)));
}

void MainWindow::setupChineseButtons(){
    vowelButtons[38]->setEnabled(false);
    vowelButtons[40]->setEnabled(false);
    vowelButtons[43]->setEnabled(false);
    vowelButtons[44]->setEnabled(false);

    vowelButtons[36]->setToolTip("close front");
    vowelButtons[37]->setToolTip("close front");
    vowelButtons[39]->setToolTip("close back");
    vowelButtons[41]->setToolTip("close-mid back");
    vowelButtons[42]->setToolTip("open front");
}

void MainWindow::setupEnglishButtons(){
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

void MainWindow::resetPlot(){
    if (accentToggle == 0){
        vowelSymbols[0]->position->setCoords(750, 295);
        vowelSymbols[1]->position->setCoords(910, 334);
        vowelSymbols[2]->position->setCoords(727, 406);
        vowelSymbols[3]->position->setCoords(830, 541);
        vowelSymbols[4]->position->setCoords(843, 652);
        vowelSymbols[5]->position->setCoords(1065, 781);
        vowelSymbols[6]->position->setCoords(1211, 784);
        vowelSymbols[7]->position->setCoords(1632, 806);
        vowelSymbols[8]->position->setCoords(1782, 766);
        vowelSymbols[9]->position->setCoords(1840, 541);
        vowelSymbols[10]->position->setCoords(2148, 434);
        vowelSymbols[11]->position->setCoords(2187, 360);
        vowelSymbols[12]->position->setCoords(2343, 294);
        for (int i = 13; i < vowelSymbols.size(); i++){
            ui->customPlot->removeItem(vowelSymbols[i]);
        }
        vowelSymbols.resize(13);
        vowelLines.resize(0);
    }
    else if (accentToggle == 1){
        vowelSymbols[0]->position->setCoords(2200, 240);
        vowelSymbols[1]->position->setCoords(2000, 235);
        vowelSymbols[2]->position->setCoords(850, 300);
        vowelSymbols[3]->position->setCoords(1350, 425);
        vowelSymbols[4]->position->setCoords(1400, 750);
        for (int i = 5; i < vowelSymbols.size(); i++){
            ui->customPlot->removeItem(vowelSymbols[i]);
        }
        vowelSymbols.resize(5);
        vowelLines.resize(0);
    }
    else if (accentToggle == 2){
        vowelSymbols[0]->position->setCoords(1850, 360);
        vowelSymbols[1]->position->setCoords(1100, 360);
        vowelSymbols[2]->position->setCoords(1950, 500);
        vowelSymbols[3]->position->setCoords(1450, 500);
        vowelSymbols[4]->position->setCoords(950, 500);
        vowelSymbols[5]->position->setCoords(1850, 600);
        vowelSymbols[6]->position->setCoords(1550, 800);
        vowelSymbols[7]->position->setCoords(1300, 800);
        for (int i = 8; i < vowelSymbols.size(); i++){
            ui->customPlot->removeItem(vowelSymbols[i]);
        }
        vowelSymbols.resize(8);
    }

    plot->replot();
}

void MainWindow::accentButtonPushed(){
    vowelToggle = !vowelToggle;
    ui->chineseGroupBox->setVisible(!vowelToggle);
    ui->englishGroupBox->setVisible(vowelToggle);
}

void MainWindow::defaultSymbolsButtonPushed(){
    if (accentToggle < 2) accentToggle++;
    else accentToggle = 0;

    for (int i = 0; i < vowelSymbols.size(); i++){
        ui->customPlot->removeItem(vowelSymbols[i]);
    }
    for (int i = 0; i < vowelLines.size(); i++){
        ui->customPlot->removeItem(vowelLines[i]);
    }

    vowelSymbols.resize(0);
    vowelLines.resize(0);

    if (accentToggle == 0) setupEnglishSymbols();
    else if (accentToggle == 1) setupChineseSymbols();
    else if (accentToggle == 2) setupEnglishReceivedSymbols();
    plot->replot();
}

void MainWindow::addSymbolButtonPushed(){
    addSymbol(ui->plainTextEdit->toPlainText());
}

void MainWindow::addSymbol(QString symbol){
    QCPItemText *userVowel = new QCPItemText(ui->customPlot);
    userVowel->position->setCoords(1500, 500);
    userVowel->setText(symbol);
    userVowel->setFont(QFont(font().family(), 40));
    userVowel->setColor(QColor(34, 34, 34));
    userVowel->setSelectedColor(Qt::blue);
    userVowel->setSelectedFont(QFont(font().family(), 40));

    vowelSymbols.push_back(userVowel);
    ui->customPlot->addItem(userVowel);
    plot->replot();
}

void MainWindow::invertAxis(){
    flags ^= INVERT_AXIS;
    plot->yAxis->setRangeReversed(!(flags & INVERT_AXIS));
    plot->replot();
}

void MainWindow::clearSymbols() {
    for (int i = 0; i < vowelSymbols.size(); i++)
        ui->customPlot->removeItem(vowelSymbols[i]);
}

void MainWindow::mouseMove(QMouseEvent *event){
    QPoint point = event->pos();
    QList<QCPAbstractItem*> selected = plot->selectedItems();
    for (int i = 0; i < vowelSymbols.size(); i++){
        if (vowelSymbols[i]->selected()){
            vowelSymbols[i]->position->setCoords(plot->xAxis->pixelToCoord(point.x()), plot->yAxis->pixelToCoord(point.y()));
            plot->replot();
        }
    }
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
}
