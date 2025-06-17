
// PlotManager.cpp
#include "PlotManager.h"
#include "AppConfig.h"
#include "FFTProcess.h"
#include "TimeDProcess.h"

#include <QPen>
#include <QPalette>
#include <QToolButton>
#include <QGridLayout>
#include <cmath>
#include <algorithm>

#include <qwt_plot_grid.h>
#include <qwt_text.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_magnifier.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_map.h>



// Constructor
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
        if (!w)
            return;

        QPalette pal = w->palette();
        // ensure tick labels use the same color as the axis titles
        pal.setColor(QPalette::WindowText, color);
        pal.setColor(QPalette::Text, color);
        w->setPalette(pal);

        // set the color of the backbone and tick marks
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

    // fix initial y-axis ranges so they don't autoscale when new data arrives
    // users can still zoom or pan to adjust the view
    fftPlot_->setAxisScale(QwtPlot::yLeft, 0.0, 8.0);      // log magnitude
    timePlot_->setAxisScale(QwtPlot::yLeft, 0.0, 150.0);   // power in uW

    new QwtPlotPanner(fftPlot_->canvas());
    new QwtPlotMagnifier(fftPlot_->canvas());
    new QwtPlotPanner(timePlot_->canvas());
    new QwtPlotMagnifier(timePlot_->canvas());

    acquireZoomButtons();
}

static inline void zoomAxis(QwtPlot *p, int axis, double factorIn)
{
    auto d = p->axisScaleDiv(axis);
    double currentMin = d.lowerBound();
    double currentMax = d.upperBound();
    double center = (currentMin + currentMax) / 2.0;
    double newHalfRange = (currentMax - currentMin) * factorIn / 2.0;

    // Compute new proposed bounds
    double newMin = center - newHalfRange;
    double newMax = center + newHalfRange;

    // Get full plot bounds from curve data
    const QwtPlotCurve* curve = nullptr;
    for (const auto *item : p->itemList(QwtPlotItem::Rtti_PlotCurve)) {
        const auto* c = dynamic_cast<const QwtPlotCurve*>(item);
        if (c && c->isVisible()) {
            curve = c;
            break;
        }
    }

    if (curve) {
        const QwtScaleMap& map = p->canvasMap(axis);
        const double dataMin = curve->boundingRect().topLeft().x();
        const double dataMax = curve->boundingRect().bottomRight().x();

        // Clamp zoom-out so we never go beyond data bounds
        if ((newMax - newMin) >= (dataMax - dataMin)) {
            newMin = dataMin;
            newMax = dataMax;
        }
    }

    p->setAxisScale(axis, newMin, newMax);
    p->replot();
}



void PlotManager::acquireZoomButtons()
{
    QWidget *fftCont  = fftPlot_->parentWidget();
    QWidget *timeCont = timePlot_->parentWidget();

    fftPlusX_  = fftCont->findChild<QToolButton*>("fftPlusX");
    fftMinusX_ = fftCont->findChild<QToolButton*>("fftMinusX");
    fftPlusY_  = fftCont->findChild<QToolButton*>("fftPlusY");
    fftMinusY_ = fftCont->findChild<QToolButton*>("fftMinusY");

    timePlusX_  = timeCont->findChild<QToolButton*>("timePlusX");
    timeMinusX_ = timeCont->findChild<QToolButton*>("timeMinusX");
    timePlusY_  = timeCont->findChild<QToolButton*>("timePlusY");
    timeMinusY_ = timeCont->findChild<QToolButton*>("timeMinusY");

    QList<QToolButton*> all { fftPlusX_, fftMinusX_, fftPlusY_, fftMinusY_,
                             timePlusX_,timeMinusX_,timePlusY_,timeMinusY_ };

    for (auto *b : all)
    {
        if (!b) { qWarning("[PlotManager] Zoom button missing in .ui !"); continue; }
        b->setFixedSize(20,20);
        b->setStyleSheet("background:#808080; color:black; border:1px solid black;");
    }

    QGridLayout *fftLayout  = qobject_cast<QGridLayout*>(fftCont->layout());
    QGridLayout *timeLayout = qobject_cast<QGridLayout*>(timeCont->layout());

    if (fftPlusX_)  { if (fftLayout) fftLayout->removeWidget(fftPlusX_);  fftPlusX_->setParent(fftPlot_); }
    if (fftMinusX_) { if (fftLayout) fftLayout->removeWidget(fftMinusX_); fftMinusX_->setParent(fftPlot_); }
    if (fftPlusY_)  { if (fftLayout) fftLayout->removeWidget(fftPlusY_);  fftPlusY_->setParent(fftPlot_); }
    if (fftMinusY_) { if (fftLayout) fftLayout->removeWidget(fftMinusY_); fftMinusY_->setParent(fftPlot_); }

    if (timePlusX_)  { if (timeLayout) timeLayout->removeWidget(timePlusX_);  timePlusX_->setParent(timePlot_); }
    if (timeMinusX_) { if (timeLayout) timeLayout->removeWidget(timeMinusX_); timeMinusX_->setParent(timePlot_); }
    if (timePlusY_)  { if (timeLayout) timeLayout->removeWidget(timePlusY_);  timePlusY_->setParent(timePlot_); }
    if (timeMinusY_) { if (timeLayout) timeLayout->removeWidget(timeMinusY_); timeMinusY_->setParent(timePlot_); }

    int margin = 5;
    if (fftPlot_)
    {
        int xRight = fftPlot_->width() - margin - 20;
        int yBottom = fftPlot_->height() - margin - 20;
        if (fftPlusX_)  fftPlusX_->move(xRight, yBottom);
        if (fftMinusX_) fftMinusX_->move(margin, yBottom);
        if (fftPlusY_)  fftPlusY_->move(xRight, margin);
        if (fftMinusY_) fftMinusY_->move(margin, margin);
    }

    if (timePlot_)
    {
        int xRight = timePlot_->width() - margin - 20;
        int yBottom = timePlot_->height() - margin - 20;
        if (timePlusX_)  timePlusX_->move(xRight, yBottom);
        if (timeMinusX_) timeMinusX_->move(margin, yBottom);
        if (timePlusY_)  timePlusY_->move(xRight, margin);
        if (timeMinusY_) timeMinusY_->move(margin, margin);
    }

    for (auto *b : all)
        if (b) b->raise();

    if (fftPlusX_)  connect(fftPlusX_,  &QToolButton::clicked, this, [this]{ zoomAxis(fftPlot_,  QwtPlot::xBottom, 0.75); });
    if (fftMinusX_) connect(fftMinusX_, &QToolButton::clicked, this, [this]{ zoomAxis(fftPlot_,  QwtPlot::xBottom, 1.25); });
    if (fftPlusY_)  connect(fftPlusY_,  &QToolButton::clicked, this, [this]{ zoomAxis(fftPlot_,  QwtPlot::yLeft,  0.75); });
    if (fftMinusY_) connect(fftMinusY_, &QToolButton::clicked, this, [this]{ zoomAxis(fftPlot_,  QwtPlot::yLeft,  1.25); });

    if (timePlusX_)  connect(timePlusX_,  &QToolButton::clicked, this, [this]{ zoomAxis(timePlot_, QwtPlot::xBottom, 0.75); });
    if (timeMinusX_) connect(timeMinusX_, &QToolButton::clicked, this, [this]{ zoomAxis(timePlot_, QwtPlot::xBottom, 1.25); });
    if (timePlusY_)  connect(timePlusY_,  &QToolButton::clicked, this, [this]{ zoomAxis(timePlot_, QwtPlot::yLeft,  0.75); });
    if (timeMinusY_) connect(timeMinusY_, &QToolButton::clicked, this, [this]{ zoomAxis(timePlot_, QwtPlot::yLeft,  1.25); });
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

void PlotManager::updateTime(const std::vector<uint16_t>& buf, double Fs, double winSec, int maxPts)
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

void PlotManager::updatePlot(FFTProcess *fft, TimeDProcess *time, bool paused, FFTMode)
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
        updateTime(tb, AppConfig::sampleRate, AppConfig::timeWindowSeconds, AppConfig::maxPointsToPlot);
    }
}



/*Future improvements:

Handle resizing, go from fixed in screen to fixed in widget


how to move around the buttons:

Because they’re manually positioned, you must move them in code — inside acquireZoomButtons():

cpp
Copy
Edit
int margin = 5;
int size = 20;

if (fftPlot_) {
    int xRight = fftPlot_->width() - margin - size;
    int yBottom = fftPlot_->height() - margin - size;

    if (fftPlusX_)  fftPlusX_->move(xRight, yBottom);      // bottom right
    if (fftMinusX_) fftMinusX_->move(margin, yBottom);     // bottom left
    if (fftPlusY_)  fftPlusY_->move(xRight, margin);       // top right
    if (fftMinusY_) fftMinusY_->move(margin, margin);      // top left
}
💡 You can adjust margin or move(x, y) coordinates to fine-tune their placement.

 */
