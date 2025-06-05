// PlotManager.h
#ifndef PLOTMANAGER_H
#define PLOTMANAGER_H

#include <QEvent>
#include <QObject>
#include <QToolButton>
#include <QVector>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_panner.h>

#include "Features.h"

class FFTProcess;
class TimeDProcess;

class PlotManager : public QObject {
  Q_OBJECT
public:
  explicit PlotManager(QwtPlot *fftPlot, QwtPlot *timePlot,
                       QObject *parent = nullptr);

  void updateFFT(const double *fftBuffer, double sampleRate);
  void updateTime(const std::vector<uint16_t> &timeBuffer, double sampleRate,
                  double timeWindowSeconds, int maxPointsToPlot);
  void updatePlot(FFTProcess *fft, TimeDProcess *time, bool isPaused,
                  FFTMode mode);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private Q_SLOTS:
  void zoomInX();
  void zoomOutX();
  void zoomInY();
  void zoomOutY();

private:
  // fetch + wire-up the buttons defined in the .ui file
  void acquireZoomButtons();
  void positionZoomButtons(QwtPlot *plot, QToolButton *plusX,
                           QToolButton *minusX, QToolButton *plusY,
                           QToolButton *minusY);

  // (re)compute allowed axis bounds from current config
  void updateBounds(double sampleRate, double timeWindowSeconds);
  void clampPlot(QwtPlot *plot, double minX, double maxX, double minY,
                 double maxY);
  void zoomAxis(QwtPlot *plot, int axis, double factor, double minBound,
                double maxBound);

  // plots & curves
  QwtPlot *fftPlot_;
  QwtPlot *timePlot_;
  QwtPlotCurve *fftCurve_;
  QwtPlotCurve *timeCurve_;

  // eight zoom buttons
  QToolButton *fftPlusX_{};
  QToolButton *fftMinusX_{};
  QToolButton *fftPlusY_{};
  QToolButton *fftMinusY_{};
  QToolButton *timePlusX_{};
  QToolButton *timeMinusX_{};
  QToolButton *timePlusY_{};
  QToolButton *timeMinusY_{};

  // panners with axis bounds
  QwtPlotPanner *fftPanner_{};
  QwtPlotPanner *timePanner_{};

  // axis bounds
  double fftMinX_{};
  double fftMaxX_{};
  double fftMinY_{0.0};
  double fftMaxY_{8.0};
  double timeMinX_{};
  double timeMaxX_{};
  double timeMinY_{0.0};
  double timeMaxY_{150.0};
};

#endif // PLOTMANAGER_H
