QT       += core gui widgets network

CONFIG   += c++11

TARGET    = TcpHexTool
TEMPLATE  = app

SOURCES  += main.cpp \
            MainWindow.cpp \
            AltSystemWorker.cpp

HEADERS  += MainWindow.h \
            AltSystemWorker.h

FORMS    += MainWindow.ui
