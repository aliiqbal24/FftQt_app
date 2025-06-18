// PlotManager.cpp
#include "PlotManager.h"
#include "AppConfig.h"
#include "FFTProcess.h"
#include "TimeDProcess.h"

#include <QPen>
#include <QPalette>
#include <QToolButton>
#include <QGridLayout>
#include <QEvent>
#include <cmath>
#include <algorithm>

#include <qwt_plot_grid.h>
#include <qwt_text.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_magnifier.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_map.h>



// Determine maximum x-axis limit based on plot type
static double getMaxXAxisLimit(QwtPlot *plot)
{
    if (plot->title().text() == "Frequency Domain")
        return (AppConfig::sampleRate >= 1e6) ? 40.0 : 100; // MHz or kHz
    if (plot->title().text() == "Time Domain")
        return AppConfig::timeWindowSeconds * 1e6;          // microseconds
    return 1e9;                                             // fallback large limit
}

// Helper to keep an axis within the data bounds of the attached curve
static inline void clampAxisToCurve(QwtPlot *plot, int axis)
{
    const QwtPlotCurve *curve = nullptr;
    for (const auto *item : plot->itemList(QwtPlotItem::Rtti_PlotCurve)) {
        const auto *c = dynamic_cast<const QwtPlotCurve *>(item);
        if (c && c->isVisible()) { curve = c; break; }
    }

    if (!curve)
        return;

    QRectF rect = curve->boundingRect();
    double dataMin, dataMax;
    if (axis == QwtPlot::yLeft || axis == QwtPlot::yRight) {
        dataMin = rect.top();
        dataMax = rect.bottom();
    } else {
        dataMin = rect.left();
        dataMax = rect.right();
    }

    if (dataMax <= dataMin)
        return;

    auto d = plot->axisScaleDiv(axis);
    double lower = d.lowerBound();
    double upper = d.upperBound();
    double width = upper - lower;

    if (width > (dataMax - dataMin)) {
        lower = dataMin;
        upper = dataMax;
    } else {
        if (lower < dataMin) { lower = dataMin; upper = lower + width; }
        if (upper > dataMax) { upper = dataMax; lower = upper - width; }
    }

    plot->setAxisScale(axis, lower, upper);
}

// Panner that keeps the view within data bounds
class BoundedPanner : public QwtPlotPanner {
public:
    explicit BoundedPanner(QwtPlot *plot)
        : QwtPlotPanner(plot->canvas()), plot_(plot) {}
protected:
    void moveCanvas(int dx, int dy) override {
        QwtPlotPanner::moveCanvas(dx, dy);
        auto d = plot_->axisScaleDiv(QwtPlot::xBottom);
        double limit = getMaxXAxisLimit(plot_);
        double min = std::max(0.0, d.lowerBound());
        double max = std::min(limit, d.upperBound());
        plot_->setAxisScale(QwtPlot::xBottom, min, max);
        plot_->replot();
    }
private:
    QwtPlot *plot_;
};

// Magnifier that keeps the view within data bounds
class BoundedMagnifier : public QwtPlotMagnifier {
public:
    explicit BoundedMagnifier(QwtPlot *plot)
        : QwtPlotMagnifier(plot->canvas()), plot_(plot) {}
protected:
    void rescale(double factor) override {
        QwtPlotMagnifier::rescale(factor);
        auto d = plot_->axisScaleDiv(QwtPlot::xBottom);
        double limit = getMaxXAxisLimit(plot_);
        double min = std::max(0.0, d.lowerBound());
        double max = std::min(limit, d.upperBound());
        plot_->setAxisScale(QwtPlot::xBottom, min, max);
        plot_->replot();
    }
private:
    QwtPlot *plot_;
};


// Constructor
PlotManager::PlotManager(QwtPlot *fftPlot, QwtPlot *timePlot, QObject *parent)
    : QObject(parent)
    , fftPlot_(fftPlot)
    , timePlot_(timePlot)
{
    QColor lightGray(200, 200, 204);
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
    timePlot_->setAxisScale(QwtPlot::yLeft, -10.0, 150.0);   // power in uW ...

    new BoundedPanner(fftPlot_);
    new BoundedMagnifier(fftPlot_);
    new BoundedPanner(timePlot_);
    new BoundedMagnifier(timePlot_);

    acquireZoomButtons();
}

void PlotManager::resetFFTView()
{
    if (!fftPlot_)
        return;

    double limit = getMaxXAxisLimit(fftPlot_);
    fftPlot_->setAxisScale(QwtPlot::xBottom, 0.0, limit);
    fftPlot_->replot();
}


static inline void zoomAxis(QwtPlot *plot, int axis, double factorIn)
{
    auto d = plot->axisScaleDiv(axis);
    double min  = d.lowerBound();
    double max  = d.upperBound();
    double center = 0.5 * (min + max);
    double halfRange = 0.5 * (max - min) * factorIn;

    double newMin = center - halfRange;
    double newMax = center + halfRange;

    if (axis == QwtPlot::xBottom) {
        double limit = getMaxXAxisLimit(plot);
        newMin = std::max(0.0, newMin);
        newMax = std::min(limit, newMax);
        if ((newMax - newMin) > limit) {
            newMin = 0.0;
            newMax = limit;
        }
    }

    plot->setAxisScale(axis, newMin, newMax);
    plot->replot();
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

    QList<QToolButton*> all { fftPlusX_, fftMinusX_, fftPlusY_, fftMinusY_, timePlusX_,timeMinusX_,timePlusY_,timeMinusY_ };

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

    if (fftPlot_)
        positionButtons(fftPlot_, fftPlusX_, fftMinusX_, fftPlusY_, fftMinusY_);
    if (timePlot_)
        positionButtons(timePlot_, timePlusX_, timeMinusX_, timePlusY_, timeMinusY_);

    if (fftPlot_)
        fftPlot_->installEventFilter(this);
    if (timePlot_)
        timePlot_->installEventFilter(this);

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
    clampAxisToCurve(timePlot_, QwtPlot::xBottom);
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

void PlotManager::positionButtons(QwtPlot *plot, QToolButton *plusX, QToolButton *minusX, QToolButton *plusY, QToolButton *minusY)
{
    if (!plot) return;

    int size = 20;

    int plotW = plot->width();
    int plotH = plot->height();

    // int verticalCenter = (plotH - size) / 2;
    int horizontalCenter = (plotW - size)/ 2;

    // (1) X-axis zoom buttons centered at bottom of plot  -- work well
    if (plusX)
        plusX->move(horizontalCenter * 1.25, plotH - size ); //
    if (minusX)
        minusX->move(horizontalCenter * 0.83, plotH - size ); //

    // (2) Y-axis zoom buttons along left edge, vertically spaced          --- WE CAN SOLVE WITH PERCENT BASED
    if (plusY)
        plusY->move(1, plotH * 0.25);  // fixed position inside plot widget
    if (minusY)
        minusY->move(1, plotH * 0.66); // adjusting spacing in screen
}

bool PlotManager::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Resize)
    {
        if (obj == fftPlot_)
            positionButtons(fftPlot_, fftPlusX_, fftMinusX_, fftPlusY_, fftMinusY_);
        else if (obj == timePlot_)
            positionButtons(timePlot_, timePlusX_, timeMinusX_, timePlusY_, timeMinusY_);
    }
    return QObject::eventFilter(obj, event);
}


