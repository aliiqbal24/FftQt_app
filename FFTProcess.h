#ifndef FFTPROCESS_H
#define FFTPROCESS_H
#define NUM_FFT_THREADS 3


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
    bool getMagnitudes(double *dst, int count);
    void setMode(FFTMode mode);

Q_SIGNALS:
    void peakFrequencyUpdated(double frequency);

private:
    FFTMode currentMode = FFTMode::FullBandwidth;
    QThread workerThread;
    pthread_t fftThreads[NUM_FFT_THREADS]; // store FFT thread handles

};

#endif // FFTPROCESS_H
