#
#  QCustomPlot Plot Examples
#

QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

TARGET = plot-examples
TEMPLATE = app

SOURCES += main.cpp\
           mainwindow.cpp \
         ../../qcustomplot.cpp

HEADERS  += mainwindow.h \
         ../../qcustomplot.h

FORMS    += mainwindow.ui

OTHER_FILES += \
    ae.points \
    augh.points \
    eeee.points \
    oooo.points \
    ../build-plot-examples-Desktop_Qt_5_2_0_GCC_64bit-Debug/a_sound.txt \
    ../build-plot-examples-Desktop_Qt_5_2_0_GCC_64bit-Debug/e_sound.txt \
    ../build-plot-examples-Desktop_Qt_5_2_0_GCC_64bit-Debug/o_sound.txt \
    ../build-plot-examples-Desktop_Qt_5_2_0_GCC_64bit-Debug/oooo2.points

