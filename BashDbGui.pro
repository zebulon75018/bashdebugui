QT += widgets
#CONFIG += c++17

TARGET = BashDbGui
TEMPLATE = app

SOURCES += \
    src/bashdbcommand.cpp \
    src/main.cpp \
    src/MainWindow.cpp \
    src/variablepanel.cpp \
    src/variablesview.cpp

HEADERS += \
    src/MainWindow.h \
    src/bashdbcommand.h \
    src/variablepanel.h \
    src/variablesview.h
