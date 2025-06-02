// PlotManager.cpp
#include "PlotManager.h"
#include "AppConfig.h"
#include "Features.h"
#include "FFTProcess.h"
#include "TimeDProcess.h"

#include <QPen>
#include <qwt_text.h>
#include <cmath>
#include <vector>
#include <algorithm>

#include <qwt_plot_grid.h>
#include <qwt_scale_widget.h>


PlotManager::PlotManager(QwtPlot *fftPlot, QwtPlot *timePlot, QObject *parent)
    : QObject(parent), fftPlot_(fftPlot), timePlot_(timePlot)
{
    QColor lightGray(183, 182, 191);
    QColor backgroundColor(13, 13, 13);
    QColor white(255, 255, 255);
    QColor neonPink(255, 112, 198);

    // FFT grid
    QwtPlotGrid *fftGrid = new QwtPlotGrid();
    fftGrid->setMajorPen(QColor(183, 182, 191, 80), 0.48);
    fftGrid->enableX(true);
    fftGrid->enableY(true);
    fftGrid->attach(fftPlot_);

    fftPlot_->setCanvasBackground(backgroundColor);
    QwtText fftTitle("Frequency Domain");
    fftTitle.setColor(lightGray);
    fftPlot_->setTitle(fftTitle);

    fftPlot_->setAxisTitle(QwtPlot::xBottom, QwtText("Frequency"));
    fftPlot_->setAxisTitle(QwtPlot::yLeft, QwtText("Log Magnitude"));
    fftPlot_->setAxisMaxMajor(QwtPlot::yLeft, 6);
    fftPlot_->setAxisScale(QwtPlot::yLeft, 0.0, 8.0);

    for (int axis = 0; axis < QwtPlot::axisCnt; ++axis) {
        if (auto *scaleWidget = fftPlot_->axisWidget(axis)) {
            QPalette pal = scaleWidget->palette();
            pal.setColor(QPalette::WindowText, lightGray);
            pal.setColor(QPalette::Text, lightGray);
            scaleWidget->setPalette(pal);
        }
    }

    // Time grid
    QwtPlotGrid *timeGrid = new QwtPlotGrid();
    timeGrid->setMajorPen(QColor(183, 182, 191, 80), 0.5);
    timeGrid->enableX(true);
    timeGrid->enableY(true);
    timeGrid->attach(timePlot_);

    timePlot_->setCanvasBackground(backgroundColor);
    QwtText timeTitle("Time Domain");
    timeTitle.setColor(lightGray);
    timePlot_->setTitle(timeTitle);

    timePlot_->setAxisTitle(QwtPlot::xBottom, QwtText("Time (\u00b5s)"));
    timePlot_->setAxisTitle(QwtPlot::yLeft, QwtText("Signal Value"));
    timePlot_->setAxisMaxMajor(QwtPlot::yLeft, 4);

    for (int axis = 0; axis < QwtPlot::axisCnt; ++axis) {
        if (auto *scaleWidget = timePlot_->axisWidget(axis)) {
            QPalette pal = scaleWidget->palette();
            pal.setColor(QPalette::WindowText, lightGray);
            pal.setColor(QPalette::Text, lightGray);
            scaleWidget->setPalette(pal);
        }
    }

    // FFT curve
    fftCurve_ = new QwtPlotCurve("FFT");
    fftCurve_->setPen(QPen(neonPink, 0.45));
    fftCurve_->setRenderHint(QwtPlotItem::RenderAntialiased, true);
    fftCurve_->attach(fftPlot_);

    // Time curve
    timeCurve_ = new QwtPlotCurve("Time Domain");
    timeCurve_->setPen(QPen(neonPink, 0.45));
    timeCurve_->attach(timePlot_);
}

void PlotManager::updateFFT(const double *fftBuffer, double sampleRate)
{
    const double binWidth_Hz = sampleRate / AppConfig::fftSize;
    QVector<double> freqs;
    QVector<double> mags_Log;

    for (int i = 0; i < AppConfig::fftBins; ++i) {
        double freq = static_cast<double>(i) * binWidth_Hz / (sampleRate > 1e6 ? 1e6 : 1e3);
        double magLin = std::max(fftBuffer[i], AppConfig::epsilon);
        freqs.append(freq);
        mags_Log.append(std::log10(magLin));
    }

    fftCurve_->setSamples(freqs, mags_Log);
    fftPlot_->setAxisTitle(QwtPlot::xBottom, QwtText(sampleRate > 1e6 ? "Frequency (MHz)" : "Frequency (KHz)"));
    fftPlot_->setAxisScale(QwtPlot::xBottom, 0.0, (sampleRate / 2.0) / (sampleRate > 1e6 ? 1e6 : 1e3));
    fftPlot_->replot();
}

void PlotManager::updateTime(const std::vector<uint16_t> &timeBuffer,
                             double sampleRate, double timeWindowSeconds, int maxPointsToPlot)
{
    const int plotSamples = static_cast<int>(timeBuffer.size());
    const int step = std::max(1, plotSamples / maxPointsToPlot);
    const double dt_us = timeWindowSeconds * 1e6 / (plotSamples - 1);

    QVector<double> timeX_us;
    QVector<double> powerY_uW;

    for (int i = 0; i < plotSamples; i += step) {
        timeX_us.append(static_cast<double>(i) * dt_us);
        double power_uW = (static_cast<double>(timeBuffer[i]) - AppConfig::adcOffset) * AppConfig::adcToMicroWatts;
        powerY_uW.append(power_uW);
    }

    timeCurve_->setSamples(timeX_us, powerY_uW);
    timePlot_->setAxisTitle(QwtPlot::yLeft, QwtText("Power (µW)"));
    timePlot_->setAxisScale(QwtPlot::yLeft, 0.0, 150.0);
    timePlot_->setAxisScale(QwtPlot::xBottom, 0.0, timeWindowSeconds * 1e6);
    timePlot_->replot();
}

void PlotManager::updatePlot(FFTProcess* fft, TimeDProcess* time, bool isPaused, FFTMode /*mode*/)
{
    if (isPaused) return;

    std::vector<double> fftBuffer(AppConfig::fftBins);
    fft->getMagnitudes(fftBuffer.data(), AppConfig::fftBins);

    if (fftBuffer[10] > 0.0)
        updateFFT(fftBuffer.data(), AppConfig::sampleRate);

    int count = time->sampleCount();
    if (count > 0) {
        std::vector<uint16_t> buffer(count);
        time->getBuffer(buffer.data(), count);
        updateTime(buffer, AppConfig::sampleRate, AppConfig::timeWindowSeconds, AppConfig::maxPointsToPlot);
    }
}
