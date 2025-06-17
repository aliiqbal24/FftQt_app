// PlotManager.h
#ifndef PLOTMANAGER_H
#define PLOTMANAGER_H

#include <QObject>
#include <QVector>
#include <QToolButton>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>

#include "Features.h"

class FFTProcess;
class TimeDProcess;

class PlotManager : public QObject {
    Q_OBJECT
public:
    explicit PlotManager(QwtPlot *fftPlot, QwtPlot *timePlot, QObject *parent = nullptr);

    void updateFFT(const double *fftBuffer, double sampleRate);

    void updateTime(const std::vector<uint16_t> &timeBuffer, double sampleRate, double timeWindowSeconds, int maxPointsToPlot);

    void updatePlot(FFTProcess *fft, TimeDProcess *time, bool isPaused, FFTMode mode);

    // Reset FFT plot view to the full bandwidth for the current sample rate
    void resetFFTView();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    // fetch + wire-up the buttons defined in the .ui file
    void acquireZoomButtons();

    void positionButtons(QwtPlot *plot, QToolButton *plusX, QToolButton *minusX,
                         QToolButton *plusY, QToolButton *minusY);

    // plots & curves
    QwtPlot        *fftPlot_;
    QwtPlot        *timePlot_;
    QwtPlotCurve   *fftCurve_;
    QwtPlotCurve   *timeCurve_;

    // eight zoom buttons
    QToolButton *fftPlusX_{};
    QToolButton *fftMinusX_{};
    QToolButton *fftPlusY_{};
    QToolButton *fftMinusY_{};
    QToolButton *timePlusX_{};
    QToolButton *timeMinusX_{};
    QToolButton *timePlusY_{};
    QToolButton *timeMinusY_{};
};

#endif // PLOTMANAGER_H
