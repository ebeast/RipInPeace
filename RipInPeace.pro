#-------------------------------------------------
#
# Project created by QtCreator 2012-09-03T13:21:44
#
#-------------------------------------------------

QT       += core gui script

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = RipInPeace
TEMPLATE = app

QMAKE_CXXFLAGS += --std=c++0x
LIBS += -lcdio_paranoia -lcdio_cdda -lcdio -lcddb -lFLAC++ -ldiscid -lmusicbrainz4

DEFINES += HAVE_LIBCDIO
debug: DEFINES += DEBUG

SOURCES += main.cpp \
    RIP.cpp \
    Paranoia.cpp \
    Settings.cpp \
    DiscInfo.cpp

HEADERS  += \
    RIP.h \
    Paranoia.h \
    DiscInfo.h \
    Settings.h

FORMS += \
    Info.ui \
    Settings.ui

RESOURCES += \
    RIP.qrc

