// plotmanager.cpp
#include "plotmanager.h"
#include "fft_config.h"

// Qt / Qwt
#include <QColor>
#include <QPen>
#include <qwt_text.h>

// Std
#include <algorithm>
#include <cmath>


#define EPSILON  1e-12    // avoids log10(0)


// constructor – sets up the two curves
PlotManager::PlotManager(QwtPlot *fftPlot,
    QwtPlot *timePlot,QObject *parent)
    : QObject(parent),
      fftPlot_(fftPlot),
      timePlot_(timePlot)
{
    QColor neonPink(255, 112, 198);

    // ---------- FFT curve
    fftCurve_ = new QwtPlotCurve("FFT");
    fftCurve_->setPen(QPen(neonPink, 0.5));
    fftCurve_->setRenderHint(QwtPlotItem::RenderAntialiased, true);
    fftCurve_->attach(fftPlot_);

    // -- Time‑domain curve
    timeCurve_ = new QwtPlotCurve("Time Domain");
    timeCurve_->setPen(QPen(neonPink, 0.5));
    timeCurve_->attach(timePlot_);
}

void PlotManager::setPause(bool paused) {
    isPaused_ = paused; }

void PlotManager::updateFFT(const double *fftBuffer, double sampleRate)
{
    if (isPaused_) return;

    /* Bin‑width depends on the *current* effective Fs. */
    const double binWidth_Hz = sampleRate / FFT_SIZE;

    QVector<double> freqs_MHz;
    QVector<double> mags_Log;

    /* No DC skip in low‑bandwidth mode any more – start at bin 0. */
    for (int i = 0; i < FFT_BINS; ++i)
    {
        double freq_MHz = static_cast<double>(i) * binWidth_Hz / 1e6;
        double magLin   = std::max(fftBuffer[i], EPSILON);

        freqs_MHz.append(freq_MHz);
        mags_Log.append(std::log10(magLin));
    }

    fftCurve_->setSamples(freqs_MHz, mags_Log);

    /* Keep x‑axis 0 → Nyquist (Fs/2).  Do this every call so the axis
       snaps instantly after a mode change. */
    fftPlot_->setAxisScale(QwtPlot::xBottom,
    0.0,(sampleRate / 2.0) / 1e6);   // MHz

    fftPlot_->replot();
}

void PlotManager::updateTime(const std::vector<uint16_t> &timeBuffer,
    double  sampleRate,double  timeWindowSeconds,int maxPointsToPlot)
{
    if (isPaused_) return;

    /* Down‑sample purely for plotting so we never push >maxPointsToPlot
       points into Qwt.  This has no impact on stored data. */
    const int plotSamples   = static_cast<int>(timeBuffer.size());
    const int step          = std::max(1, plotSamples / maxPointsToPlot);
    const double dt_us      = 1e6 / sampleRate;

    QVector<double> timeX_us;
    QVector<double> powerY_uW;

    for (int i = 0; i < plotSamples; i += step)
    {
        timeX_us.append(static_cast<double>(i) * dt_us);

        // power to Uw, double check this conversion, guessed so far
        double power_uW = (static_cast<double>(timeBuffer[i]) - 49555.0)* 0.0147;
        powerY_uW.append(power_uW);
    }

    timeCurve_->setSamples(timeX_us, powerY_uW);

    timePlot_->setAxisTitle(QwtPlot::yLeft, QwtText("Power (\u00B5W)"));
    timePlot_->setAxisScale(QwtPlot::yLeft, 0.0, 150.0);
    timePlot_->setAxisScale(QwtPlot::xBottom, 0.0, timeWindowSeconds * 1e6);

    timePlot_->replot();
}
