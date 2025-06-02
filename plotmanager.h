// PlotManager.h
#ifndef PLOTMANAGER_H
#define PLOTMANAGER_H

#include <QObject>
#include <QVector>
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
    void updateTime(const std::vector<uint16_t> &timeBuffer,
    double sampleRate, double timeWindowSeconds, int maxPointsToPlot);
    void updatePlot(FFTProcess* fft, TimeDProcess* time, bool isPaused, FFTMode mode);

private:
    QwtPlot *fftPlot_;
    QwtPlot *timePlot_;
    QwtPlotCurve *fftCurve_;
    QwtPlotCurve *timeCurve_;
};

#endif // PLOTMANAGER_H
