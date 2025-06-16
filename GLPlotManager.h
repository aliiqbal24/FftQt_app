#ifndef GLPLOTMANAGER_H
#define GLPLOTMANAGER_H

#include <QObject>
#include <QVector>
#include "Features.h"

class FFTProcess;
class TimeDProcess;
class FFTGLWidget;
class TimeGLWidget;

class GLPlotManager : public QObject
{
    Q_OBJECT
public:
    explicit GLPlotManager(FFTGLWidget* fftWidget, TimeGLWidget* timeWidget, QObject* parent = nullptr);

    void updatePlot(FFTProcess* fft, TimeDProcess* time, bool isPaused, FFTMode mode);

private:
    void updateFFT(const double* fftBuffer, double sampleRate);
    void updateTime(const std::vector<uint16_t>& timeBuffer, double sampleRate, double timeWindowSeconds, int maxPoints);

    FFTGLWidget*  m_fftWidget;
    TimeGLWidget* m_timeWidget;
};

#endif // GLPLOTMANAGER_H
