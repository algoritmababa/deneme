QT       += core gui widgets network

CONFIG   += c++11

TARGET    = TcpHexTool
TEMPLATE  = app

SOURCES  += main.cpp \
            MainWindow.cpp \
            AltSystemParameters.cpp

HEADERS  += MainWindow.h \
            AltSystemParameters.h

FORMS    += MainWindow.ui
