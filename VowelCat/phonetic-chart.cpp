// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#include <inttypes.h>
#include <stdio.h>
#include <wchar.h>

#include <QString>
#include <QVector>

#include "phonetic-chart.h"
#include "qcustomplot.h"

VowelSymbol::VowelSymbol(QCustomPlot *plot, const wchar_t *symbol, uint32_t f1,
                         uint32_t f2):
    QCPItemText(plot)
{
    QFont font(plot->font().family(), FONT_SIZE);

    position->setCoords(f2, f1);
    setFont(font);
    setSelectedFont(font);
    setSelectedColor(Qt::blue);
    setText(QString::fromWCharArray(symbol));
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
    char *title_;

    if (fscanf(stream, "%ms\n", &title_) != 1)
        return false;

    title = QString(title_);
    free(title_);

    return true;
}

bool PhoneticChart::loadSymbols(FILE *stream) {
    uint32_t f1, f2;
    uint32_t endx, endy;

    // Support a maximum two-character symbol, and make sure it's always
    // null-terminated.
    wchar_t symbol[3];
    symbol[2] = L'\0';

    while (fscanf(stream, "%2lc %u %u", symbol, &f1, &f2) == 3) {
        // If the symbol is only one character, the second character will be the
        // whitespace separator. In that case, strip the whitespace off.
        if (iswspace(symbol[1]))
            symbol[1] = L'\0';

        symbols.push_back(new VowelSymbol(plot, symbol, f1, f2));

        if (fscanf(stream, "%u %u\n", &endx, &endy) != 2)
            continue;

        arrows.push_back(new DipthongArrow(plot, symbols.last(),
            QPointF(endx, endy)));
    }

    return true;
}

void PhoneticChart::install() {
    label->setText(title);

    for (int i = 0; i < symbols.size(); i += 1)
        plot->addItem(symbols[i]);

    for (int i = 0; i < arrows.size(); i += 1)
        plot->addItem(arrows[i]);
}

void PhoneticChart::uninstall() {
    for (int i = 0; i < symbols.size(); i++)
        plot->removeItem(symbols[i]);

    for (int i = 0; i < arrows.size(); i += 1)
        plot->removeItem(arrows[i]);
}

void PhoneticChart::save(FILE *stream) {
    for (int i = 0; i < symbols.size(); i += 1) {
        auto vs = symbols[i];
        auto coords = vs->position->coords();

        fprintf(stream, "%lc\t%u\t%u\n", vs->text().at(0).unicode(),
            (uint32_t) coords.y(), (uint32_t) coords.x());
    }
}

