#ifndef FFTGLWIDGET_H
#define FFTGLWIDGET_H

#include <QOpenGLWidget>
#include <QVector>
#include <QOpenGLFunctions>

class FFTGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit FFTGLWidget(QWidget *parent = nullptr);

    void setFFTData(const QVector<double> &x, const QVector<double> &y);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QVector<double> m_x;
    QVector<double> m_y;
};

#endif // FFTGLWIDGET_H
