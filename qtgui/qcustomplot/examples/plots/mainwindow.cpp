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

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

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

  int xpoint;
  int ypoint;
  ifstream points;

  vector<string> pointFiles;
  pointFiles.push_back("oooo.points");
  pointFiles.push_back("eeee.points");
  pointFiles.push_back("augh.points");
  pointFiles.push_back("ae.points");
  pointFiles.push_back("a_sound.txt");
  pointFiles.push_back("e_sound.txt");
  pointFiles.push_back("o_sound.txt");
  pointFiles.push_back("oooo2.points");

  for (uint i = 0; i < pointFiles.size(); i++){
    points.open(pointFiles[i].c_str());
    if (points.is_open()) {
      points >> xpoint;
      points >> ypoint;
      while (!points.eof()){
        x.append(xpoint);
        y.append(ypoint);
        points >> xpoint;
        points >> ypoint;
      }
      points.close();
    }
  }


  customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
  QCPGraph *graph = customPlot->addGraph();
  frame = -1;
  graph->setData(x, y);
  graph->setPen(QPen(Qt::white));
  graph->rescaleKeyAxis();

  customPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
  customPlot->xAxis->setRange(3000, 0);
  customPlot->xAxis->setRangeReversed(true);
  customPlot->xAxis->setLabel("F2 (Hz)");

  customPlot->yAxis->setRange(3000, 0);
  customPlot->yAxis->setRangeReversed(true);
  customPlot->yAxis->setLabel("F1 (Hz)");

  // add the phase tracer (red circle) which sticks to the graph data (and gets updated in bracketDataSlot by timer event):
  QCPItemTracer *phaseTracer = new QCPItemTracer(customPlot);
  customPlot->addItem(phaseTracer);
  tracer = phaseTracer; // so we can access it later in the bracketDataSlot for animation
  phaseTracer->setGraph(graph);
  phaseTracer->setGraphKey(832);
  phaseTracer->setInterpolating(false);
  phaseTracer->setStyle(QCPItemTracer::tsCircle);
  phaseTracer->setPen(QPen(Qt::red));
  phaseTracer->setBrush(Qt::red);
  phaseTracer->setSize(30);

  QCPItemTracer *phaseTracerLagging = new QCPItemTracer(customPlot);
  customPlot->addItem(phaseTracerLagging);
  laggingTracer = phaseTracerLagging; // so we can access it later in the bracketDataSlot for animation
  phaseTracerLagging->setGraph(graph);
  phaseTracerLagging->setGraphKey(1263);
  phaseTracerLagging->setInterpolating(false);
  phaseTracerLagging->setStyle(QCPItemTracer::tsCircle);
  phaseTracerLagging->setPen(QPen(Qt::red));
  phaseTracerLagging->setBrush(Qt::red);
  phaseTracerLagging->setSize(20);

  QCPItemTracer *phaseTracerLagging2 = new QCPItemTracer(customPlot);
  customPlot->addItem(phaseTracerLagging2);
  laggingTracer2 = phaseTracerLagging2; // so we can access it later in the bracketDataSlot for animation
  phaseTracerLagging2->setGraph(graph);
  phaseTracerLagging2->setGraphKey(755);
  phaseTracerLagging2->setInterpolating(false);
  phaseTracerLagging2->setStyle(QCPItemTracer::tsCircle);
  phaseTracerLagging2->setPen(QPen(Qt::red));
  phaseTracerLagging2->setBrush(Qt::red);
  phaseTracerLagging2->setSize(14);

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

  frame++;
  //ui->customPlot->graph()->setData(x, y);

  tracer->setGraphKey(x[frame%x.size()]);

  if (frame%x.size() < 1) laggingTracer->setGraphKey(x[x.size()-1]);
  else laggingTracer->setGraphKey(x[frame%x.size() - 1]);

  if (frame%x.size() < 2) laggingTracer2->setGraphKey(x[x.size()-2]);
  else laggingTracer2->setGraphKey(x[frame%x.size() - 2]);

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
