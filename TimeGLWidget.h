#ifndef TIMEGLWIDGET_H
#define TIMEGLWIDGET_H

#include <QOpenGLWidget>
#include <QVector>
#include <QOpenGLFunctions>

class TimeGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit TimeGLWidget(QWidget *parent = nullptr);

    void setTimeData(const QVector<double> &x, const QVector<double> &y);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QVector<double> m_x;
    QVector<double> m_y;
};

#endif // TIMEGLWIDGET_H
