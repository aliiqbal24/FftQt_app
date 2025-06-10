// PlotManager.h
#ifndef PLOTMANAGER_H
#define PLOTMANAGER_H

#include <QObject>
#include <QVector>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <QToolButton>
#include <QEvent>
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

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private Q_SLOTS:
    void zoomInX();
    void zoomOutX();
    void zoomInY();
    void zoomOutY();

private:
    void createZoomButtons(QwtPlot *plot,
                           QToolButton *&plusX, QToolButton *&minusX,
                           QToolButton *&plusY, QToolButton *&minusY);
    void positionZoomButtons(QwtPlot *plot,
                             QToolButton *plusX, QToolButton *minusX,
                             QToolButton *plusY, QToolButton *minusY);

private:
    QwtPlot *fftPlot_;
    QwtPlot *timePlot_;
    QwtPlotCurve *fftCurve_;
    QwtPlotCurve *timeCurve_;

    // Zoom buttons
    QToolButton *fftPlusX_;
    QToolButton *fftMinusX_;
    QToolButton *fftPlusY_;
    QToolButton *fftMinusY_;

    QToolButton *timePlusX_;
    QToolButton *timeMinusX_;
    QToolButton *timePlusY_;
    QToolButton *timeMinusY_;

    double fftXMin_;
    double fftXMax_;
    double timeXMin_;
    double timeXMax_;
};

#endif // PLOTMANAGER_H
