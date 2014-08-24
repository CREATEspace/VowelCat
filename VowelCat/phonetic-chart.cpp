// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#include <inttypes.h>
#include <stdio.h>

#include <QString>
#include <QVector>

#include "phonetic-chart.h"
#include "qcustomplot.h"

VowelSymbol::VowelSymbol(QCustomPlot *plot, const char *symbol, uint32_t f1,
                         uint32_t f2):
    VowelSymbol(plot, QString::fromUtf8(symbol) ,f1, f2)
{}

VowelSymbol::VowelSymbol(QCustomPlot *plot, const QString &symbol, uint32_t f1,
                         uint32_t f2):
    VowelSymbol(plot, f1, f2)
{
    setText(symbol);
}

VowelSymbol::VowelSymbol(QCustomPlot *plot, uint32_t f1,
                         uint32_t f2):
    QCPItemText(plot)
{
    QFont font(plot->font().family(), FONT_SIZE);

    position->setCoords(f2, f1);
    setFont(font);
    setSelectedFont(font);
    setSelectedColor(Qt::blue);
}

DipthongArrow::DipthongArrow(QCustomPlot *plot, VowelSymbol *start_,
                             const QPointF &end_):
    QCPItemLine(plot)
{
    start->setParentAnchor(start_->position);
    end->setCoords(end_);

    QPen pen;
    pen.setWidth(4);

    setHead(QCPLineEnding::esSpikeArrow);
    setPen(pen);;
    setSelectable(false);
}

PhoneticChart::PhoneticChart(QCustomPlot *plot_, QLabel *label_):
    plot(plot_),
    label(label_)
{}

PhoneticChart::PhoneticChart():
    plot(NULL),
    label(NULL)
{}

bool PhoneticChart::load(FILE *stream) {
    return loadTitle(stream) && loadSymbols(stream);
}

bool PhoneticChart::loadTitle(FILE *stream) {
// Clang doesn't support the 'm' allocation flag, so OSX is stuck with fixed-
// length titles. GCC targets can have arbitrary-length titles.
#if !defined(__MACH__) && !defined(__MINGW32__)
    char *title_;

    if (fscanf(stream, "%m[^\n]\n", &title_) != 1)
        return false;

    title = QString(title_);
    free(title_);
#else
    char title_[256];

    if (fscanf(stream, "%256[^\n]\n", title_) != 1)
        return false;

    title = QString(title_);
#endif

    return true;
}

bool PhoneticChart::loadSymbols(FILE *stream) {
    uint32_t f1, f2;
    uint32_t endx, endy;
    char symbol[9] = {'\0'};

    while (fscanf(stream, "%8s %u %u", symbol, &f1, &f2) == 3) {
        symbols.push_back(new VowelSymbol(plot, symbol, f1, f2));

        if (fscanf(stream, "%u %u\n", &endy, &endx) != 2)
            continue;

        arrows.push_back(new DipthongArrow(plot, symbols.last(),
            QPointF(endx, endy)));
        symbolArrowMap.insert(symbols.last(), arrows.last());
    }

    return true;
}

void PhoneticChart::install() {
    label->setText(title);

    for (int i = 0; i < symbols.size(); i += 1){
        plot->addItem(symbols[i]);
    }

    for (int i = 0; i < arrows.size(); i += 1){
        plot->addItem(arrows[i]);
    }
    show();
}

void PhoneticChart::uninstall() {
    for (int i = 0; i < symbols.size(); i++){
        plot->mItems.removeOne(symbols[i]);
    }

    for (int i = 0; i < arrows.size(); i += 1){
        plot->mItems.removeOne(arrows[i]);
    }
    hide();
}

void PhoneticChart::save(FILE *stream) {
    {
        auto utf8 = title.toUtf8();
        fprintf(stream, "%s\n", utf8.data());
    }

    for (int i = 0; i < symbols.size(); i += 1) {
        auto vs = symbols[i];

        {
            auto text = vs->text();
            auto utf8 = text.toUtf8();
            fprintf(stream, "%s\t", utf8.data());
        }

        {
            auto coords = vs->position->coords();
            fprintf(stream, "%u\t%u", (uint32_t) coords.y(),
                (uint32_t) coords.x());
        }

        auto cursor = symbolArrowMap.find(vs);

        if (cursor != symbolArrowMap.end()) {
            auto da = cursor.value();
            auto coords = da->end->coords();

            fprintf(stream, "\t%u\t%u", (uint32_t) coords.x(),
                (uint32_t) coords.y());
        }

        fputc('\n', stream);
    }
}

void PhoneticChart::mouseMove(QMouseEvent *event){
    QPoint point = event->pos();
    QList<QCPAbstractItem*> selected = plot->selectedItems();
    for (int i = 0; i < symbols.size(); i++){
         if (symbols[i]->selected()){
             symbols[i]->position->setCoords(plot->xAxis->pixelToCoord(point.x()), plot->yAxis->pixelToCoord(point.y()));
             plot->replot();
         }
    }
}

void PhoneticChart::addSymbol(QString &symbol, uint32_t f1, uint32_t f2){
    auto vs = new VowelSymbol(plot, symbol, f1, f2);
    symbols.push_back(vs);
    plot->addItem(vs);
}

void PhoneticChart::hide(){
    for (int i = 0; i < symbols.size(); i++){
            symbols[i]->setVisible(false);
    }

    for (int i = 0; i < arrows.size(); i += 1){
        arrows[i]->setVisible(false);
    }
}

void PhoneticChart::show(){
    for (int i = 0; i < symbols.size(); i++){
        symbols[i]->setVisible(true);
    }

    for (int i = 0; i < arrows.size(); i += 1){
        arrows[i]->setVisible(true);
    }
}
