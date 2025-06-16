#include "GLPlotManager.h"
#include "FFTGLWidget.h"
#include "TimeGLWidget.h"
#include "FFTProcess.h"
#include "TimeDProcess.h"
#include "AppConfig.h"
#include <vector>
#include <algorithm>
#include <cmath>

GLPlotManager::GLPlotManager(FFTGLWidget* fftWidget, TimeGLWidget* timeWidget, QObject* parent)
    : QObject(parent)
    , m_fftWidget(fftWidget)
    , m_timeWidget(timeWidget)
{
}

void GLPlotManager::updateFFT(const double* fftBuffer, double sampleRate)
{
    QVector<double> freqs;
    QVector<double> mags;
    double binWidth = sampleRate / AppConfig::fftSize;
    for (int i=0; i<AppConfig::fftBins; ++i) {
        double freq = static_cast<double>(i) * binWidth / (sampleRate>1e6 ? 1e6 : 1e3);
        double mag = std::log10(std::max(fftBuffer[i], AppConfig::epsilon));
        freqs.append(freq);
        mags.append(mag);
    }
    if (m_fftWidget)
        m_fftWidget->setFFTData(freqs, mags);
}

void GLPlotManager::updateTime(const std::vector<uint16_t>& timeBuffer, double sampleRate, double timeWindowSeconds, int maxPoints)
{
    int samples = static_cast<int>(timeBuffer.size());
    int step = std::max(1, samples / maxPoints);
    double dt_us = timeWindowSeconds * 1e6 / (samples - 1 + 1e-9);

    QVector<double> x;
    QVector<double> y;
    for (int i=0; i<samples; i+=step) {
        x.append(i * dt_us);
        double power = (static_cast<double>(timeBuffer[i]) - AppConfig::adcOffset) * AppConfig::adcToMicroWatts;
        y.append(power);
    }
    if (m_timeWidget)
        m_timeWidget->setTimeData(x, y);
}

void GLPlotManager::updatePlot(FFTProcess* fft, TimeDProcess* time, bool isPaused, FFTMode /*mode*/)
{
    if (isPaused)
        return;

    std::vector<double> fftBuf(AppConfig::fftBins);
    fft->getMagnitudes(fftBuf.data(), AppConfig::fftBins);
    if (fftBuf[10] > 0.0)
        updateFFT(fftBuf.data(), AppConfig::sampleRate);

    int count = time->sampleCount();
    if (count > 0) {
        std::vector<uint16_t> buf(count);
        time->getBuffer(buf.data(), count);
        updateTime(buf, AppConfig::sampleRate, AppConfig::timeWindowSeconds, AppConfig::maxPointsToPlot);
    }
}

