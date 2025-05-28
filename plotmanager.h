#ifndef PLOTMANAGER_H
#define PLOTMANAGER_H

// Qt / Qwt
#include <QObject>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>

// Std
#include <vector>
#include <cstdint>

/*!
 * \brief Lightweight helper that owns and refreshes one FFT curve and
 *        one time‑domain curve.
 */
class PlotManager : public QObject
{
    Q_OBJECT
public:
    explicit PlotManager(QwtPlot *fftPlot,
    QwtPlot *timePlot, QObject *parent = nullptr);

    void setPause(bool paused);


    void updateFFT(const double *fftBuffer, double sampleRate);

    /*! Refresh the time‑domain curve. */
    void updateTime(const std::vector<uint16_t> &timeBuffer,
    double  sampleRate,double  timeWindowSeconds,int maxPointsToPlot);

private:
    QwtPlot       *fftPlot_;
    QwtPlot       *timePlot_;
    QwtPlotCurve  *fftCurve_;
    QwtPlotCurve  *timeCurve_;
    bool           isPaused_ = false;
};

#endif // PLOTMANAGER_H

