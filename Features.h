// Features.h
#ifndef FEATURES_H
#define FEATURES_H

#include <QString>
#include <vector>
#include <cstdint>
#include "AppConfig.h"
#include <QWidget>
#include <QLabel>

enum class FFTMode { FullBandwidth = 0, LowBandwidth = 1 };

class Features {
public:
    static void togglePause(bool &isPaused);
    static void switchMode(FFTMode &mode);

    static void saveFFTPlot(const QString &fileName, const double *fftBuffer, double sampleRate);

    static void saveTimePlot(const QString &fileName,const std::vector<uint16_t> &buffer,double sampleRate, double timeWindowSeconds);

    static void promptUserToSavePlot(QWidget *parent,const double *fftBuffer,const std::vector<uint16_t> &timeBuffer);

    static void updatePeakFrequency(QLabel *label, FFTMode mode, double frequency, bool isPaused);
};

#endif // FEATURES_H
