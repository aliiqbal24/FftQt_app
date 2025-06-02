QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
CONFIG += no_keywords  # Forces use of Q_SLOT, Q_SIGNAL, Q_EMIT

TEMPLATE = app
TARGET = FFT_Qwt_Plotter

# === Source Files ===
SOURCES += \
    FFTProcess.cpp \
    Features.cpp \
    TimeDProcess.cpp \
    fft_config.cpp \
    main.cpp \
    mainwindow.cpp \
    plotmanager.cpp

# === Header Files ===
HEADERS += \
    AppConfig.h \
    FFTProcess.h \
    Features.h \
    TimeDProcess.h \
    mainwindow.h \
    plotmanager.h

# === UI Forms ===
FORMS += \
    mainwindow.ui

# === Include Paths ===
INCLUDEPATH += \
    $$PWD \
    $$PWD/Plot_dependencies/libri-0.9.5/include \
    $$PWD/Plot_dependencies/mkl/latest/include \
    $$PWD/Plot_dependencies/mkl/latest/include/fftw \
    C:/Ultracoustics-ALI-Playground/qwt-6.3.0/src

# === Link Libraries ===
LIBS += \
    -L$$PWD/Plot_dependencies/libri-0.9.5/lib64 -lri \
    -L$$PWD/Plot_dependencies/mkl/latest/lib -lmkl_rt \
    -LC:/Ultracoustics-ALI-Playground/qwt-6.3.0/build/Desktop_Qt_6_9_0_MinGW_64_bit-Debug/lib -lqwtd \
    -lpthread -lm

RESOURCES += \
    icons.qrc
