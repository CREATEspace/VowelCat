QT += core gui

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets printsupport
}

TARGET = qtGui
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    qcustomplot.cpp \
    formant-worker.cpp \

HEADERS += \
    mainwindow.h \
    qcustomplot.h \
    formant-worker.h \

FORMS += \
    mainwindow.ui \

QMAKE_CXXFLAGS += -std=c++11
QMAKE_CXXFLAGS += $$(CFLAGS)
LIBS += $(LDFLAGS)
