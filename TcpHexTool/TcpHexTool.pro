QT       += core gui widgets network

CONFIG   += c++11

TARGET    = TcpHexTool
TEMPLATE  = app

SOURCES  += main.cpp \
            MainWindow.cpp \
            AltSystemWorker.cpp \
            AltSystemParameters.cpp

HEADERS  += MainWindow.h \
            AltSystemWorker.h \
            AltSystemParameters.h

FORMS    += MainWindow.ui
