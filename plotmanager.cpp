
// PlotManager.cpp
#include "PlotManager.h"
#include "AppConfig.h"
#include "FFTProcess.h"
#include "TimeDProcess.h"

#include <QEvent>
#include <QPalette>
#include <QPen>
#include <QToolButton>
#include <algorithm>
#include <cmath>

#include <qwt_plot_grid.h>
#include <qwt_plot_magnifier.h>
#include <qwt_plot_panner.h>
#include <qwt_scale_widget.h>
#include <qwt_text.h>

// Helper panner that clamps movement within given axis bounds
class BoundedPanner : public QwtPlotPanner {
public:
  BoundedPanner(QwtPlot *plot, double minX, double maxX, double minY,
                double maxY)
      : QwtPlotPanner(plot->canvas()), plot_(plot), minX_(minX), maxX_(maxX),
        minY_(minY), maxY_(maxY) {}

  void setBounds(double minX, double maxX, double minY, double maxY) {
    minX_ = minX;
    maxX_ = maxX;
    minY_ = minY;
    maxY_ = maxY;
  }

protected:
  void moveCanvas(int dx, int dy) override {
    QwtPlotPanner::moveCanvas(dx, dy);
    clamp();
  }

private:
  void clamp() {
    auto clampAxis = [&](int axis, double lo, double hi) {
      auto d = plot_->axisScaleDiv(axis);
      double lower = d.lowerBound();
      double upper = d.upperBound();
      double span = upper - lower;
      if (lower < lo) {
        lower = lo;
        upper = lower + span;
      }
      if (upper > hi) {
        upper = hi;
        lower = upper - span;
      }
      if (span > hi - lo) {
        lower = lo;
        upper = hi;
      }
      plot_->setAxisScale(axis, lower, upper);
    };
    clampAxis(QwtPlot::xBottom, minX_, maxX_);
    clampAxis(QwtPlot::yLeft, minY_, maxY_);
    plot_->replot();
  }

  QwtPlot *plot_;
  double minX_, maxX_, minY_, maxY_;
};

/* ------------------------------------------------------------------ *
 *                       C O N S T R U C T O R                        *
 * ------------------------------------------------------------------ */
PlotManager::PlotManager(QwtPlot *fftPlot, QwtPlot *timePlot, QObject *parent)
    : QObject(parent), fftPlot_(fftPlot), timePlot_(timePlot) {
  QColor lightGray(183, 182, 191);
  QColor backgroundColor(13, 13, 13);
  QColor neonPink(255, 112, 198);

  auto makeGrid = [&](QwtPlot *plot) {
    QwtPlotGrid *g = new QwtPlotGrid;
    g->setMajorPen(QColor(183, 182, 191, 80), 0.48);
    g->enableX(true);
    g->enableY(true);
    g->attach(plot);
  };
  makeGrid(fftPlot_);
  makeGrid(timePlot_);

  fftPlot_->setCanvasBackground(backgroundColor);
  timePlot_->setCanvasBackground(backgroundColor);

  {
    QwtText t("Frequency Domain");
    t.setColor(lightGray);
    fftPlot_->setTitle(t);
  }
  {
    QwtText t("Time Domain");
    t.setColor(lightGray);
    timePlot_->setTitle(t);
  }

  {
    QwtText xTitle("Frequency");
    QwtText yTitle("Log Magnitude");
    xTitle.setColor(lightGray);
    yTitle.setColor(lightGray);
    fftPlot_->setAxisTitle(QwtPlot::xBottom, xTitle);
    fftPlot_->setAxisTitle(QwtPlot::yLeft, yTitle);
  }
  {
    QwtText xTitle("Time (µs)");
    QwtText yTitle("Signal Value");
    xTitle.setColor(lightGray);
    yTitle.setColor(lightGray);
    timePlot_->setAxisTitle(QwtPlot::xBottom, xTitle);
    timePlot_->setAxisTitle(QwtPlot::yLeft, yTitle);
  }

  auto setAxisColor = [&](QwtPlot *plot, int axis, const QColor &color) {
    QwtScaleWidget *w = plot->axisWidget(axis);
    if (!w)
      return;

    QPalette pal = w->palette();
    // ensure tick labels use the same color as the axis titles
    pal.setColor(QPalette::WindowText, color);
    pal.setColor(QPalette::Text, color);
    w->setPalette(pal);

    // set the color of the backbone and tick marks
    w->setColor(color);

    QwtText title = w->title();
    title.setColor(color);
    w->setTitle(title);
  };

  setAxisColor(fftPlot_, QwtPlot::xBottom, lightGray);
  setAxisColor(fftPlot_, QwtPlot::yLeft, lightGray);
  setAxisColor(timePlot_, QwtPlot::xBottom, lightGray);
  setAxisColor(timePlot_, QwtPlot::yLeft, lightGray);

  fftCurve_ = new QwtPlotCurve("FFT");
  fftCurve_->setPen(QPen(neonPink, 0.45));
  fftCurve_->attach(fftPlot_);

  timeCurve_ = new QwtPlotCurve("Time");
  timeCurve_->setPen(QPen(neonPink, 0.45));
  timeCurve_->attach(timePlot_);

  // fix initial y-axis ranges so they don't autoscale when new data arrives
  // users can still zoom or pan to adjust the view
  fftPlot_->setAxisScale(QwtPlot::yLeft, 0.0, 8.0);    // log magnitude
  timePlot_->setAxisScale(QwtPlot::yLeft, 0.0, 150.0); // power in uW

  updateBounds(AppConfig::sampleRate, AppConfig::timeWindowSeconds);

  fftPanner_ =
      new BoundedPanner(fftPlot_, fftMinX_, fftMaxX_, fftMinY_, fftMaxY_);
  timePanner_ =
      new BoundedPanner(timePlot_, timeMinX_, timeMaxX_, timeMinY_, timeMaxY_);

  new QwtPlotMagnifier(fftPlot_->canvas());
  new QwtPlotMagnifier(timePlot_->canvas());

  fftPlot_->installEventFilter(this);
  timePlot_->installEventFilter(this);
}

void PlotManager::attachZoomButtons(QToolButton *fftPlusX, QToolButton *fftMinusX,
                                    QToolButton *fftPlusY, QToolButton *fftMinusY,
                                    QToolButton *timePlusX, QToolButton *timeMinusX,
                                    QToolButton *timePlusY, QToolButton *timeMinusY) {
  fftPlusX_ = fftPlusX;
  fftMinusX_ = fftMinusX;
  fftPlusY_ = fftPlusY;
  fftMinusY_ = fftMinusY;

  timePlusX_ = timePlusX;
  timeMinusX_ = timeMinusX;
  timePlusY_ = timePlusY;
  timeMinusY_ = timeMinusY;

  QList<QToolButton *> all{fftPlusX_,  fftMinusX_,  fftPlusY_,  fftMinusY_,
                           timePlusX_, timeMinusX_, timePlusY_, timeMinusY_};

  for (auto *b : all) {
    if (!b) {
      qWarning("[PlotManager] Zoom button missing in .ui !");
      continue;
    }
    b->setFixedSize(20, 20);
    b->setStyleSheet(
        "background:#808080; color:black; border:1px solid black;");
  }

  connect(fftPlusX_, &QToolButton::clicked, this, &PlotManager::zoomInX);
  connect(fftMinusX_, &QToolButton::clicked, this, &PlotManager::zoomOutX);
  connect(fftPlusY_, &QToolButton::clicked, this, &PlotManager::zoomInY);
  connect(fftMinusY_, &QToolButton::clicked, this, &PlotManager::zoomOutY);
  connect(timePlusX_, &QToolButton::clicked, this, &PlotManager::zoomInX);
  connect(timeMinusX_, &QToolButton::clicked, this, &PlotManager::zoomOutX);
  connect(timePlusY_, &QToolButton::clicked, this, &PlotManager::zoomInY);
  connect(timeMinusY_, &QToolButton::clicked, this, &PlotManager::zoomOutY);

  positionZoomButtons(fftPlot_, fftPlusX_, fftMinusX_, fftPlusY_, fftMinusY_);
  positionZoomButtons(timePlot_, timePlusX_, timeMinusX_, timePlusY_,
                      timeMinusY_);
}

void PlotManager::positionZoomButtons(QwtPlot *plot, QToolButton *plusX,
                                      QToolButton *minusX, QToolButton *plusY,
                                      QToolButton *minusY) {
  if (!plot)
    return;
  auto *yW = plot->axisWidget(QwtPlot::yLeft);
  auto *xW = plot->axisWidget(QwtPlot::xBottom);
  if (!yW || !xW)
    return;

  const int pad = 2;
  QPoint y0 = yW->geometry().topLeft();
  plusY->move(y0.x() - plusY->width() - pad, y0.y());
  minusY->move(y0.x() - minusY->width() - pad, y0.y() + plusY->height() + pad);

  QPoint x0 = xW->geometry().bottomLeft();
  minusX->move(x0.x(), x0.y() + pad);
  plusX->move(x0.x() + minusX->width() + pad, x0.y() + pad);
}

bool PlotManager::eventFilter(QObject *obj, QEvent *e) {
  if (e->type() == QEvent::Resize) {
    if (obj == fftPlot_)
      positionZoomButtons(fftPlot_, fftPlusX_, fftMinusX_, fftPlusY_,
                          fftMinusY_);
    else if (obj == timePlot_)
      positionZoomButtons(timePlot_, timePlusX_, timeMinusX_, timePlusY_,
                          timeMinusY_);
  }
  return QObject::eventFilter(obj, e);
}

void PlotManager::updateBounds(double sampleRate, double timeWindowSeconds) {
  fftMinX_ = 0.0;
  fftMaxX_ = (sampleRate / 2.0) / (sampleRate > 1e6 ? 1e6 : 1e3);
  timeMinX_ = 0.0;
  timeMaxX_ = timeWindowSeconds * 1e6;
  if (fftPanner_)
    static_cast<BoundedPanner *>(fftPanner_)
        ->setBounds(fftMinX_, fftMaxX_, fftMinY_, fftMaxY_);
  if (timePanner_)
    static_cast<BoundedPanner *>(timePanner_)
        ->setBounds(timeMinX_, timeMaxX_, timeMinY_, timeMaxY_);
}

void PlotManager::clampPlot(QwtPlot *plot, double minX, double maxX,
                            double minY, double maxY) {
  auto clampAxis = [&](int axis, double lo, double hi) {
    auto d = plot->axisScaleDiv(axis);
    double l = d.lowerBound();
    double u = d.upperBound();
    double span = u - l;
    if (l < lo) {
      l = lo;
      u = l + span;
    }
    if (u > hi) {
      u = hi;
      l = u - span;
    }
    if (span > hi - lo) {
      l = lo;
      u = hi;
    }
    plot->setAxisScale(axis, l, u);
  };

  clampAxis(QwtPlot::xBottom, minX, maxX);
  clampAxis(QwtPlot::yLeft, minY, maxY);
}

void PlotManager::zoomAxis(QwtPlot *plot, int axis, double factor,
                           double minBound, double maxBound) {
  auto d = plot->axisScaleDiv(axis);
  double c = (d.lowerBound() + d.upperBound()) / 2.0;
  double h = (d.upperBound() - d.lowerBound()) * factor;
  double l = c - h / 2;
  double u = c + h / 2;
  double span = u - l;
  if (l < minBound) {
    l = minBound;
    u = l + span;
  }
  if (u > maxBound) {
    u = maxBound;
    l = u - span;
  }
  if (span > maxBound - minBound) {
    l = minBound;
    u = maxBound;
  }
  plot->setAxisScale(axis, l, u);
  plot->replot();
  if (plot == fftPlot_)
    positionZoomButtons(fftPlot_, fftPlusX_, fftMinusX_, fftPlusY_, fftMinusY_);
  else if (plot == timePlot_)
    positionZoomButtons(timePlot_, timePlusX_, timeMinusX_, timePlusY_,
                        timeMinusY_);
}

void PlotManager::updateFFT(const double *buf, double Fs) {
  const double binW = Fs / AppConfig::fftSize;
  QVector<double> x, y;
  x.reserve(AppConfig::fftBins);
  y.reserve(AppConfig::fftBins);

  for (int i = 0; i < AppConfig::fftBins; ++i) {
    x.append(i * binW / (Fs > 1e6 ? 1e6 : 1e3));
    y.append(std::log10(std::max(buf[i], AppConfig::epsilon)));
  }

  fftCurve_->setSamples(x, y);
  fftPlot_->setAxisTitle(QwtPlot::xBottom,
                         Fs > 1e6 ? "Frequency (MHz)" : "Frequency (kHz)");

  updateBounds(Fs, AppConfig::timeWindowSeconds);
  clampPlot(fftPlot_, fftMinX_, fftMaxX_, fftMinY_, fftMaxY_);
  fftPlot_->replot();
}

void PlotManager::updateTime(const std::vector<uint16_t> &buf, double Fs,
                             double winSec, int maxPts) {
  const int N = static_cast<int>(buf.size());
  if (N == 0)
    return;

  int step = std::max(1, N / maxPts);
  const double dt_us = winSec * 1e6 / (N - 1);

  QVector<double> x, y;
  x.reserve(N / step + 4);
  y.reserve(N / step + 4);

  for (int i = 0; i < N; i += step) {
    x.append(i * dt_us);
    double p = (static_cast<double>(buf[i]) - AppConfig::adcOffset) *
               AppConfig::adcToMicroWatts;
    y.append(p);
  }
  timeCurve_->setSamples(x, y);
  timePlot_->setAxisTitle(QwtPlot::yLeft, "Power (µW)");

  updateBounds(Fs, winSec);
  clampPlot(timePlot_, timeMinX_, timeMaxX_, timeMinY_, timeMaxY_);
  timePlot_->replot();
}

void PlotManager::updatePlot(FFTProcess *fft, TimeDProcess *time, bool paused,
                             FFTMode) {
  if (paused)
    return;

  std::vector<double> fftBuf(AppConfig::fftBins);
  if (fft->getMagnitudes(fftBuf.data(), AppConfig::fftBins))
    updateFFT(fftBuf.data(), AppConfig::sampleRate);

  int n = time->sampleCount();
  if (n > 0) {
    std::vector<uint16_t> tb(n);
    time->getBuffer(tb.data(), n);
    updateTime(tb, AppConfig::sampleRate, AppConfig::timeWindowSeconds,
               AppConfig::maxPointsToPlot);
  }
}

void PlotManager::zoomInX() {
  QwtPlot *p = qobject_cast<QwtPlot *>(sender()->parent());
  if (!p)
    return;
  double minB = (p == fftPlot_) ? fftMinX_ : timeMinX_;
  double maxB = (p == fftPlot_) ? fftMaxX_ : timeMaxX_;
  zoomAxis(p, QwtPlot::xBottom, 0.5, minB, maxB);
}

void PlotManager::zoomOutX() {
  QwtPlot *p = qobject_cast<QwtPlot *>(sender()->parent());
  if (!p)
    return;
  double minB = (p == fftPlot_) ? fftMinX_ : timeMinX_;
  double maxB = (p == fftPlot_) ? fftMaxX_ : timeMaxX_;
  zoomAxis(p, QwtPlot::xBottom, 1.6, minB, maxB);
}

void PlotManager::zoomInY() {
  QwtPlot *p = qobject_cast<QwtPlot *>(sender()->parent());
  if (!p)
    return;
  double minB = (p == fftPlot_) ? fftMinY_ : timeMinY_;
  double maxB = (p == fftPlot_) ? fftMaxY_ : timeMaxY_;
  zoomAxis(p, QwtPlot::yLeft, 0.5, minB, maxB);
}

void PlotManager::zoomOutY() {
  QwtPlot *p = qobject_cast<QwtPlot *>(sender()->parent());
  if (!p)
    return;
  double minB = (p == fftPlot_) ? fftMinY_ : timeMinY_;
  double maxB = (p == fftPlot_) ? fftMaxY_ : timeMaxY_;
  zoomAxis(p, QwtPlot::yLeft, 1.6, minB, maxB);
}
