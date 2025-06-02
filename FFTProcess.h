#ifndef FFTPROCESS_H
#define FFTPROCESS_H

#include <QObject>
#include <QThread>
#include <atomic>
#include <cstdint>
#include <vector>
#include "Features.h"  // for FFTMode

class FFTProcess : public QObject {
    Q_OBJECT

public:
    explicit FFTProcess(QObject *parent = nullptr);
    ~FFTProcess();

    void start();
    void getMagnitudes(double *dst, int count);
    void setMode(FFTMode mode);

Q_SIGNALS:
    void peakFrequencyUpdated(double frequency);

private:
    FFTMode currentMode = FFTMode::FullBandwidth;
    QThread workerThread;
};

#endif // FFTPROCESS_H
