#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "plotmanager.h"
#include "fft_mode.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private Q_SLOTS:
    void togglePause();
    void promptUserToSavePlot();
    void saveTimePlotToFile();
    void saveFFTPlotToFile();
    void onModeChanged(int index);
    void restartStreamsForMode();
    int computeRequiredSampleCount() const;


private:
    void updatePlot();

    Ui::MainWindow *ui;
    PlotManager    *plotManager;
    bool            isPaused = false;
    FFTMode         currentMode;
};

#endif // MAINWINDOW_H
