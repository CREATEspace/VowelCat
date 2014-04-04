# Copyright 2014 Formant Industries. See the Copying file at the top-level
# directory of this project.

QT += core gui

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets printsupport
}

TARGET = qtGui
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    plotter.cpp \
    qcustomplot.cpp \
    timespec.cpp \

HEADERS += \
    mainwindow.h \
    plotter.h \
    qcustomplot.h \
    timespec.h \

FORMS += \
    mainwindow.ui \

QMAKE_CXXFLAGS += -std=c++11
QMAKE_CXXFLAGS += $$(CFLAGS)
LIBS += $(LDFLAGS)
