#include "FFTGLWidget.h"
#include <QPainter>
#include <QtMath>

FFTGLWidget::FFTGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
}

void FFTGLWidget::setFFTData(const QVector<double> &x, const QVector<double> &y)
{
    m_x = x;
    m_y = y;
    update();
}

void FFTGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
}

void FFTGLWidget::resizeGL(int w, int h)
{
    Q_UNUSED(w);
    Q_UNUSED(h);
}

void FFTGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(13,13,13));
    painter.setPen(QColor(255,112,198));

    QVector<QPointF> points;
    if (!m_x.isEmpty() && m_x.size() == m_y.size()) {
        for (int i=0; i<m_x.size(); ++i) {
            double x = m_x[i];
            double y = m_y[i];
            double px = (x - m_x.first()) / (m_x.last()-m_x.first()+1e-9) * width();
            double py = height() - ((y + 1.0) / 2.0) * height();
            points.append(QPointF(px, py));
        }
    } else {
        // draw dummy sine wave
        int n=100;
        for (int i=0;i<n;++i) {
            double t = (double)i/(n-1);
            double x = t*width();
            double y = height()/2.0 - qSin(t*6.2831853)*height()/4.0;
            points.append(QPointF(x,y));
        }
    }
    if (points.size()>1)
        painter.drawPolyline(points.constData(), points.size());
}

