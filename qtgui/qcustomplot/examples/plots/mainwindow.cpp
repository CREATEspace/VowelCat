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

  QVector<int> x(108), y(108);
  x[0] = 742;
  x[1] = 738;
  x[2] = 755;
  x[3] = 740;
  x[4] = 731;
  x[5] = 746;
  x[6] = 734;
  x[7] = 772;
  x[8] = 738;
  x[9] = 751;
  x[10] = 755;
  x[11] = 755;
  x[12] = 762;
  x[13] = 790;
  x[14] = 759;
  x[15] = 753;
  x[16] = 758;
  x[17] = 764;
  x[18] = 793;
  x[19] = 786;
  x[20] = 782;
  x[21] = 752;
  x[22] = 777;
  x[23] = 749;
  x[24] = 761;
  x[25] = 750;
  x[26] = 762;
  x[27] = 1816;
  x[28] = 1819;
  x[29] = 1809;
  x[30] = 1807;
  x[31] = 1801;
  x[32] = 1797;
  x[33] = 1793;
  x[34] = 1788;
  x[35] = 1790;
  x[36] = 1787;
  x[37] = 1781;
  x[38] = 1786;
  x[39] = 1783;
  x[40] = 1780;
  x[41] = 1779;
  x[42] = 1772;
  x[43] = 1766;
  x[44] = 1754;
  x[45] = 1742;
  x[46] = 1725;
  x[47] = 1714;
  x[48] = 1710;
  x[49] = 1708;
  x[50] = 1706;
  x[51] = 1709;
  x[52] = 1710;
  x[53] = 1715;
  x[54] = 747;
  x[55] = 766;
  x[56] = 736;
  x[57] = 709;
  x[58] = 689;
  x[59] = 696;
  x[60] = 698;
  x[61] = 680;
  x[62] = 698;
  x[63] = 684;
  x[64] = 744;
  x[65] = 738;
  x[66] = 737;
  x[67] = 801;
  x[68] = 801;
  x[69] = 819;
  x[70] = 834;
  x[71] = 780;
  x[72] = 840;
  x[73] = 845;
  x[74] = 856;
  x[75] = 893;
  x[76] = 903;
  x[77] = 914;
  x[78] = 937;
  x[79] = 921;
  x[80] = 915;
  x[81] = 2335;
  x[82] = 2398;
  x[83] = 2395;
  x[84] = 2385;
  x[85] = 2395;
  x[86] = 2387;
  x[87] = 2395;
  x[88] = 2392;
  x[89] = 2392;
  x[90] = 2373;
  x[91] = 2379;
  x[92] = 2372;
  x[93] = 2372;
  x[94] = 2369;
  x[95] = 2368;
  x[96] = 2360;
  x[97] = 2366;
  x[98] = 2353;
  x[99] = 2354;
  x[100] = 2347;
  x[101] = 2354;
  x[102] = 2359;
  x[103] = 2354;
  x[104] = 2349;
  x[105] = 2350;
  x[106] = 2356;
  x[107] = 2348;

  y[0] = 295;
  y[1] = 297;
  y[2] = 301;
  y[3] = 299;
  y[4] = 295;
  y[5] = 300;
  y[6] = 298;
  y[7] = 309;
  y[8] = 303;
  y[9] = 303;
  y[10] = 311;
  y[11] = 309;
  y[12] = 313;
  y[13] = 313;
  y[14] = 309;
  y[15] = 304;
  y[16] = 304;
  y[17] = 304;
  y[18] = 307;
  y[19] = 308;
  y[20] = 305;
  y[21] = 301;
  y[22] = 301;
  y[23] = 301;
  y[24] = 299;
  y[25] = 301;
  y[26] = 299;
  y[27] = 720;
  y[28] = 724;
  y[29] = 724;
  y[30] = 721;
  y[31] = 726;
  y[32] = 728;
  y[33] = 730;
  y[34] = 735;
  y[35] = 742;
  y[36] = 744;
  y[37] = 745;
  y[38] = 744;
  y[39] = 750;
  y[40] = 738;
  y[41] = 746;
  y[42] = 758;
  y[43] = 742;
  y[44] = 750;
  y[45] = 752;
  y[46] = 742;
  y[47] = 745;
  y[48] = 746;
  y[49] = 734;
  y[50] = 732;
  y[51] = 725;
  y[52] = 720;
  y[53] = 714;
  y[54] = 606;
  y[55] = 619;
  y[56] = 651;
  y[57] = 628;
  y[58] = 620;
  y[59] = 659;
  y[60] = 603;
  y[61] = 612;
  y[62] = 636;
  y[63] = 673;
  y[64] = 671;
  y[65] = 665;
  y[66] = 680;
  y[67] = 661;
  y[68] = 658;
  y[69] = 664;
  y[70] = 654;
  y[71] = 673;
  y[72] = 654;
  y[73] = 657;
  y[74] = 659;
  y[75] = 661;
  y[76] = 662;
  y[77] = 660;
  y[78] = 663;
  y[79] = 657;
  y[80] = 652;
  y[81] = 297;
  y[82] = 286;
  y[83] = 295;
  y[84] = 287;
  y[85] = 289;
  y[86] = 284;
  y[87] = 285;
  y[88] = 285;
  y[89] = 286;
  y[90] = 286;
  y[91] = 289;
  y[92] = 289;
  y[93] = 291;
  y[94] = 290;
  y[95] = 292;
  y[96] = 290;
  y[97] = 293;
  y[98] = 289;
  y[99] = 293;
  y[100] = 292;
  y[101] = 295;
  y[102] = 296;
  y[103] = 293;
  y[104] = 294;
  y[105] = 291;
  y[106] = 288;
  y[107] = 292;


  frame++;
  QVector<double> activex(1), activey(1);
  activex[0] = x[frame%107];
  activey[0] = y[frame%107];
  ui->customPlot->graph()->setData(activex, activey);

  itemDemoPhaseTracer->setGraphKey(x[frame%107]);

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
