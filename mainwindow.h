// MainWindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "Features.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class FFTProcess;
class TimeDProcess;
class PlotManager; // old Qwt plot manager (unused)
class GLPlotManager;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::OpenGLWindow *ui;
    FFTProcess     *fft;
    TimeDProcess   *time;
    // PlotManager    *plotManager; // Qwt version
    GLPlotManager  *plotManager;
    bool            isPaused = false;
    FFTMode         currentMode = FFTMode::FullBandwidth;
};

#endif // MAINWINDOW_H
