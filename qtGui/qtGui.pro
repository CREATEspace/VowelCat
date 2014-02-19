QT += core gui

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets printsupport
}

TARGET = qtGui
TEMPLATE = app

SOURCES += \
    main.cpp\
    mainwindow.cpp \
    qcustomplot.cpp \

HEADERS += \
    mainwindow.h \
    qcustomplot.h \

FORMS += \
    mainwindow.ui \

OTHER_FILES += \
    pointData/ae.points \
    pointData/augh.points \
    pointData/eeee.points \
    pointData/oooo.points \

QMAKE_CFLAGS += $$(CFLAGS)
QMAKE_CXXFLAGS += $$(CFLAGS)
QMAKE_LFLAGS += $$(LDFLAGS)
