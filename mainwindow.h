// MainWindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QKeyEvent>
#include "Features.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class FFTProcess;
class TimeDProcess;
class PlotManager;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    Ui::MainWindow *ui;
    FFTProcess     *fft;
    TimeDProcess   *time;
    PlotManager    *plotManager;
    bool            isPaused = true;
    FFTMode         currentMode = FFTMode::FullBandwidth;
    bool            isFullScreen = false;

private Q_SLOTS:

    void openUltracousticsSite();

    void            toggleFullScreen();
};

#endif // MAINWINDOW_H
