
// PlotManager.cpp
#include "PlotManager.h"
#include "AppConfig.h"
#include "FFTProcess.h"
#include "TimeDProcess.h"

#include <QPen>
#include <QPalette>
#include <QToolButton>
#include <QEvent>
#include <cmath>
#include <algorithm>

#include <qwt_plot_grid.h>
#include <qwt_text.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_magnifier.h>
#include <qwt_scale_widget.h>

/* ------------------------------------------------------------------ *
 *                       C O N S T R U C T O R                        *
 * ------------------------------------------------------------------ */
PlotManager::PlotManager(QwtPlot *fftPlot, QwtPlot *timePlot, QObject *parent)
    : QObject(parent)
    , fftPlot_(fftPlot)
    , timePlot_(timePlot)
{
    QColor lightGray(183, 182, 191);
    QColor backgroundColor(13, 13, 13);
    QColor neonPink(255, 112, 198);

    auto makeGrid = [&](QwtPlot *plot){
        QwtPlotGrid *g = new QwtPlotGrid;
        g->setMajorPen(QColor(183,182,191,80), 0.48);
        g->enableX(true); g->enableY(true); g->attach(plot);
    };
    makeGrid(fftPlot_); makeGrid(timePlot_);

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
        fftPlot_->setAxisTitle(QwtPlot::yLeft,   yTitle);
    }
    {
        QwtText xTitle("Time (µs)");
        QwtText yTitle("Signal Value");
        xTitle.setColor(lightGray);
        yTitle.setColor(lightGray);
        timePlot_->setAxisTitle(QwtPlot::xBottom, xTitle);
        timePlot_->setAxisTitle(QwtPlot::yLeft,   yTitle);
    }

    auto setAxisColor = [&](QwtPlot *plot, int axis, const QColor &color) {
        QwtScaleWidget *w = plot->axisWidget(axis);
        if (!w) return;
        QPalette pal = w->palette();
        pal.setColor(QPalette::WindowText, color);
        w->setPalette(pal);
        QwtText title = w->title();
        title.setColor(color);
        w->setTitle(title);
    };

    setAxisColor(fftPlot_, QwtPlot::xBottom, lightGray);
    setAxisColor(fftPlot_, QwtPlot::yLeft,   lightGray);
    setAxisColor(timePlot_, QwtPlot::xBottom, lightGray);
    setAxisColor(timePlot_, QwtPlot::yLeft,   lightGray);

    fftCurve_  = new QwtPlotCurve("FFT");
    fftCurve_->setPen(QPen(neonPink,0.45));
    fftCurve_->attach(fftPlot_);

    timeCurve_ = new QwtPlotCurve("Time");
    timeCurve_->setPen(QPen(neonPink,0.45));
    timeCurve_->attach(timePlot_);

    new QwtPlotPanner(fftPlot_->canvas());
    new QwtPlotMagnifier(fftPlot_->canvas());
    new QwtPlotPanner(timePlot_->canvas());
    new QwtPlotMagnifier(timePlot_->canvas());

    acquireZoomButtons();

    fftPlot_->installEventFilter(this);
    timePlot_->installEventFilter(this);
}

void PlotManager::acquireZoomButtons()
{
    fftPlusX_  = fftPlot_->findChild<QToolButton*>("fftPlusX");
    fftMinusX_ = fftPlot_->findChild<QToolButton*>("fftMinusX");
    fftPlusY_  = fftPlot_->findChild<QToolButton*>("fftPlusY");
    fftMinusY_ = fftPlot_->findChild<QToolButton*>("fftMinusY");

    timePlusX_  = timePlot_->findChild<QToolButton*>("timePlusX");
    timeMinusX_ = timePlot_->findChild<QToolButton*>("timeMinusX");
    timePlusY_  = timePlot_->findChild<QToolButton*>("timePlusY");
    timeMinusY_ = timePlot_->findChild<QToolButton*>("timeMinusY");

    QList<QToolButton*> all { fftPlusX_, fftMinusX_, fftPlusY_, fftMinusY_,
                             timePlusX_,timeMinusX_,timePlusY_,timeMinusY_ };

    for (auto *b : all)
    {
        if (!b) { qWarning("[PlotManager] Zoom button missing in .ui !"); continue; }
        b->setFixedSize(20,20);
        b->setStyleSheet("background:#808080; color:black; border:1px solid black;");
    }

    connect(fftPlusX_,  &QToolButton::clicked, this, &PlotManager::zoomInX);
    connect(fftMinusX_, &QToolButton::clicked, this, &PlotManager::zoomOutX);
    connect(fftPlusY_,  &QToolButton::clicked, this, &PlotManager::zoomInY);
    connect(fftMinusY_, &QToolButton::clicked, this, &PlotManager::zoomOutY);
    connect(timePlusX_,  &QToolButton::clicked, this, &PlotManager::zoomInX);
    connect(timeMinusX_, &QToolButton::clicked, this, &PlotManager::zoomOutX);
    connect(timePlusY_,  &QToolButton::clicked, this, &PlotManager::zoomInY);
    connect(timeMinusY_, &QToolButton::clicked, this, &PlotManager::zoomOutY);

    positionZoomButtons(fftPlot_,  fftPlusX_,  fftMinusX_,  fftPlusY_,  fftMinusY_);
    positionZoomButtons(timePlot_, timePlusX_, timeMinusX_, timePlusY_, timeMinusY_);
}

void PlotManager::positionZoomButtons(QwtPlot *plot,
                                      QToolButton *plusX,  QToolButton *minusX,
                                      QToolButton *plusY,  QToolButton *minusY)
{
    if (!plot) return;
    auto *yW = plot->axisWidget(QwtPlot::yLeft);
    auto *xW = plot->axisWidget(QwtPlot::xBottom);
    if (!yW || !xW) return;

    const int pad = 2;
    QPoint y0 = yW->geometry().topLeft();
    plusY ->move(y0.x() - plusY->width()  - pad, y0.y());
    minusY->move(y0.x() - minusY->width() - pad, y0.y() + plusY->height() + pad);

    QPoint x0 = xW->geometry().bottomLeft();
    minusX->move(x0.x(),                   x0.y() + pad);
    plusX ->move(x0.x() + minusX->width() + pad, x0.y() + pad);
}

bool PlotManager::eventFilter(QObject *obj, QEvent *e)
{
    if (e->type() == QEvent::Resize)
    {
        if (obj == fftPlot_)
            positionZoomButtons(fftPlot_,  fftPlusX_,  fftMinusX_,  fftPlusY_,  fftMinusY_);
        else if (obj == timePlot_)
            positionZoomButtons(timePlot_, timePlusX_, timeMinusX_, timePlusY_, timeMinusY_);
    }
    return QObject::eventFilter(obj, e);
}

void PlotManager::updateFFT(const double *buf, double Fs)
{
    const double binW = Fs / AppConfig::fftSize;
    QVector<double> x, y;
    x.reserve(AppConfig::fftBins); y.reserve(AppConfig::fftBins);

    for (int i=0;i<AppConfig::fftBins;++i)
    {
        x.append(i * binW / (Fs>1e6 ? 1e6 : 1e3));
        y.append(std::log10(std::max(buf[i], AppConfig::epsilon)));
    }

    fftCurve_->setSamples(x,y);
    fftPlot_->setAxisTitle(QwtPlot::xBottom,
                           Fs>1e6 ? "Frequency (MHz)" : "Frequency (kHz)");
    fftPlot_->replot();
}

void PlotManager::updateTime(const std::vector<uint16_t>& buf,
                             double Fs, double winSec, int maxPts)
{
    const int N = static_cast<int>(buf.size());
    if (N==0) return;

    int step = std::max(1, N / maxPts);
    const double dt_us = winSec * 1e6 / (N-1);

    QVector<double> x, y; x.reserve(N/step+4); y.reserve(N/step+4);

    for (int i=0;i<N;i+=step)
    {
        x.append(i * dt_us);
        double p = (static_cast<double>(buf[i]) - AppConfig::adcOffset)
                   * AppConfig::adcToMicroWatts;
        y.append(p);
    }
    timeCurve_->setSamples(x,y);
    timePlot_->setAxisTitle(QwtPlot::yLeft, "Power (µW)");
    timePlot_->replot();
}

void PlotManager::updatePlot(FFTProcess *fft, TimeDProcess *time,
                             bool paused, FFTMode)
{
    if (paused) return;

    std::vector<double> fftBuf(AppConfig::fftBins);
    if (fft->getMagnitudes(fftBuf.data(), AppConfig::fftBins))
        updateFFT(fftBuf.data(), AppConfig::sampleRate);

    int n = time->sampleCount();
    if (n>0)
    {
        std::vector<uint16_t> tb(n);
        time->getBuffer(tb.data(), n);
        updateTime(tb, AppConfig::sampleRate, AppConfig::timeWindowSeconds,
                   AppConfig::maxPointsToPlot);
    }
}

static inline void zoomAxis(QwtPlot *p, int axis, double factorIn)
{
    auto d = p->axisScaleDiv(axis);
    double c = (d.lowerBound() + d.upperBound()) / 2.0;
    double h = (d.upperBound() - d.lowerBound()) * factorIn;
    p->setAxisScale(axis, c - h/2, c + h/2);
    p->replot();
}

void PlotManager::zoomInX()  { zoomAxis(qobject_cast<QwtPlot*>(sender()->parent()), QwtPlot::xBottom, 0.5); }
void PlotManager::zoomOutX() { zoomAxis(qobject_cast<QwtPlot*>(sender()->parent()), QwtPlot::xBottom, 1.6); }
void PlotManager::zoomInY()  { zoomAxis(qobject_cast<QwtPlot*>(sender()->parent()), QwtPlot::yLeft,  0.5); }
void PlotManager::zoomOutY() { zoomAxis(qobject_cast<QwtPlot*>(sender()->parent()), QwtPlot::yLeft,  1.6); }
