/***************************************************************************
**                                                                        **
**  QCustomPlot, an easy to use, modern plotting widget for Qt            **
**  Copyright (C) 2011, 2012, 2013 Emanuel Eichhammer                     **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Author: Emanuel Eichhammer                                   **
**  Website/Contact: http://www.qcustomplot.com/                          **
**             Date: 09.12.13                                             **
**          Version: 1.1.1                                                **
****************************************************************************/

/************************************************************************************************************
**                                                                                                         **
**  This is the example code for QCustomPlot.                                                              **
**                                                                                                         **
**  It demonstrates basic and some advanced capabilities of the widget. The interesting code is inside     **
**  the "setup(...)Demo" functions of MainWindow.                                                          **
**                                                                                                         **
**  In order to see a demo in action, call the respective "setup(...)Demo" function inside the             **
**  MainWindow constructor. Alternatively you may call setupDemo(i) where i is the index of the demo       **
**  you want (for those, see MainWindow constructor comments). All other functions here are merely a       **
**  way to easily create screenshots of all demos for the website. I.e. a timer is set to successively     **
**  setup all the demos and make a screenshot of the window area and save it in the ./screenshots          **
**  directory.                                                                                             **
**                                                                                                         **
*************************************************************************************************************/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QDesktopWidget>
#include <QScreen>
#include <QMessageBox>
#include <QMetaEnum>

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  setGeometry(400, 250, 1000, 800);

  //setupDemo(15);
  setupItemDemo(ui->customPlot);
  setWindowTitle("QCustomPlot: "+demoName);
  statusBar()->clearMessage();
  currentDemoIndex = 15;
  ui->customPlot->replot();
}

void MainWindow::setupItemDemo(QCustomPlot *customPlot)
{
#if QT_VERSION < QT_VERSION_CHECK(4, 7, 0)
  QMessageBox::critical(this, "", "You're using Qt < 4.7, the animation of the item demo needs functions that are available with Qt 4.7 to work properly");
#endif

  demoName = "Item Demo";

  customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
  QCPGraph *graph = customPlot->addGraph();
  frame = 0;
  QVector<double> x(1), y(1);
  x[0] = 750;
  y[0] = 350;
  graph->setData(x, y);
  graph->setPen(QPen(Qt::blue));
  graph->rescaleKeyAxis();

  customPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
  customPlot->xAxis->setRange(2500, 600);
  customPlot->xAxis->setRangeReversed(true);
  customPlot->xAxis->setLabel("F2 (Hz)");

  customPlot->yAxis->setRange(1000, 200);
  customPlot->yAxis->setRangeReversed(true);
  customPlot->yAxis->setLabel("F1 (Hz)");

  // add the phase tracer (red circle) which sticks to the graph data (and gets updated in bracketDataSlot by timer event):
  QCPItemTracer *phaseTracer = new QCPItemTracer(customPlot);
  customPlot->addItem(phaseTracer);
  itemDemoPhaseTracer = phaseTracer; // so we can access it later in the bracketDataSlot for animation
  phaseTracer->setGraph(graph);
  phaseTracer->setGraphKey(350);
  phaseTracer->setInterpolating(false);
  phaseTracer->setStyle(QCPItemTracer::tsCircle);
  phaseTracer->setPen(QPen(Qt::red));
  phaseTracer->setBrush(Qt::red);
  phaseTracer->setSize(20);

  // setup a timer that repeatedly calls MainWindow::realtimeDataSlot:
  connect(&dataTimer, SIGNAL(timeout()), this, SLOT(bracketDataSlot()));
  dataTimer.start(100); // Interval 0 means to refresh as fast as possible
}

void MainWindow::realtimeDataSlot(){}

void MainWindow::bracketDataSlot()
{
#if QT_VERSION < QT_VERSION_CHECK(4, 7, 0)
  double secs = 0;
#else
  double secs = QDateTime::currentDateTime().toMSecsSinceEpoch()/1000.0;
#endif

  QVector<int> ooox(27), oooy(27);
  ooox[0] = 742;
  ooox[1] = 738;
  ooox[2] = 755;
  ooox[3] = 740;
  ooox[4] = 731;
  ooox[5] = 746;
  ooox[6] = 734;
  ooox[7] = 772;
  ooox[8] = 738;
  ooox[9] = 751;
  ooox[10] = 755;
  ooox[11] = 755;
  ooox[12] = 762;
  ooox[13] = 790;
  ooox[14] = 759;
  ooox[15] = 753;
  ooox[16] = 758;
  ooox[17] = 764;
  ooox[18] = 793;
  ooox[19] = 786;
  ooox[20] = 782;
  ooox[21] = 752;
  ooox[22] = 777;
  ooox[23] = 749;
  ooox[24] = 761;
  ooox[25] = 750;
  ooox[26] = 762;

  oooy[0] = 295;
  oooy[1] = 297;
  oooy[2] = 301;
  oooy[3] = 299;
  oooy[4] = 295;
  oooy[5] = 300;
  oooy[6] = 298;
  oooy[7] = 309;
  oooy[8] = 303;
  oooy[9] = 303;
  oooy[10] = 311;
  oooy[11] = 309;
  oooy[12] = 313;
  oooy[13] = 313;
  oooy[14] = 309;
  oooy[15] = 304;
  oooy[16] = 304;
  oooy[17] = 304;
  oooy[18] = 307;
  oooy[19] = 308;
  oooy[20] = 305;
  oooy[21] = 301;
  oooy[22] = 301;
  oooy[23] = 301;
  oooy[24] = 299;
  oooy[25] = 301;
  oooy[26] = 299;

  frame++;
  QVector<double> x(1), y(1);
  x[0] = ooox[frame%27];
  y[0] = oooy[frame%27];
  ui->customPlot->graph()->setData(x, y);

  itemDemoPhaseTracer->setGraphKey(ooox[frame%27]);

  ui->customPlot->replot();

  // calculate frames per second:
  double key = secs;
  static double lastFpsKey;
  static int frameCount;
  ++frameCount;
  if (key-lastFpsKey > 2) // average fps over 2 seconds
  {
    ui->statusBar->showMessage(
          QString("%1 FPS, Total Data points: %2, %3 executions")
          .arg(frameCount/(key-lastFpsKey), 0, 'f', 0)
          .arg(ui->customPlot->graph(0)->data()->count())
          .arg(frame)
          , 0);
    lastFpsKey = key;
    frameCount = 0;
  }
}

void MainWindow::setupPlayground(QCustomPlot *customPlot)
{
  Q_UNUSED(customPlot)
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::screenShot(){}

void MainWindow::allScreenShots(){}
