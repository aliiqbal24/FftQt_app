// mainwindow.cpp
// Built-in C++ Libs
#include <cmath>
#include <vector>
#include <algorithm>

// Qt Includes
#include <QPen>
#include <QInputDialog>
#include <QTimer>
#include <QThread>
#include <QDebug>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QSplitter>

// Qwt Includes
#include <qwt_plot_grid.h>
#include <qwt_text.h>
#include <qwt_scale_widget.h>
#include <qwt_plot_curve.h>

// Project Headers
#include "fft_config.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "fft_mode.h"
#include "plotmanager.h"

// Adjustable Plotting Parameters
double sampleRate  = 80e6;    // Sample rate of hardware (80 MSPS)
double timeWindowSeconds = 100e-6;  // Time window (adjustable by user)
int maxPointsToPlot = 10000;    // Max points to plot
int plotRefreshRateMs = 30;      // Plot refresh rate in milliseconds

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    plotManager = new PlotManager(ui->FFT_plot, ui->Time_plot, this);
    currentMode = FFTMode::FullBandwidth;

    QColor lightGray(183, 182, 191);
    QColor backgroundColor(13, 13, 13);
    QColor white(255, 255, 255);

    ui->PausePlay->setIcon(QIcon(":/play-pause-icon.png"));
    ui->PausePlay->setIconSize(QSize(48, 48));
    connect(ui->PausePlay, &QPushButton::clicked, this, &MainWindow::togglePause);

    ui->Save->setIcon(QIcon(":/download-icon.png"));
    ui->Save->setIconSize(QSize(48, 48));
    connect(ui->Save, &QPushButton::clicked, this, &MainWindow::promptUserToSavePlot);


    // mode switcher UI
    ui->modes->setStyleSheet("QComboBox { color: white; background-color: rgb(95, 95, 95); border: 1px solid gray; } QComboBox QAbstractItemView { background-color: rgb(95, 95, 95); color: white; }");
    connect(ui->modes, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onModeChanged);

    // Peak frequency tracker UI
    ui->PeakFreq->setText("Peak: --");
    ui->PeakFreq->setStyleSheet("color: gray; font-size: 18px;");
    ui->PeakFreq->setAlignment(Qt::AlignCenter);


    QList<int> initialSizes { height() / 2, height() / 2 };
    ui->splitter->setSizes(initialSizes);
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setChildrenCollapsible(true);

    ui->FFT_plot->setMinimumHeight(0);
    ui->Time_plot->setMinimumHeight(0);
    ui->FFT_plot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->Time_plot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QwtPlotGrid *fftGrid = new QwtPlotGrid();
    fftGrid->setMajorPen(QColor(183, 182, 191, 80), 0.48);
    fftGrid->enableX(true);
    fftGrid->enableY(true);
    fftGrid->attach(ui->FFT_plot);

    ui->FFT_plot->setAxisMaxMajor(QwtPlot::yLeft, 6);
    ui->FFT_plot->setCanvasBackground(backgroundColor);
    QwtText fftTitle("Frequency Domain");
    fftTitle.setColor(white);
    ui->FFT_plot->setTitle(fftTitle);
    QwtText fftX("Frequency");
    fftX.setColor(lightGray);
    ui->FFT_plot->setAxisTitle(QwtPlot::xBottom, fftX);
    QwtText fftY("Log Magnitude");
    fftY.setColor(lightGray);
    ui->FFT_plot->setAxisTitle(QwtPlot::yLeft, fftY);
    ui->FFT_plot->setAxisScale(QwtPlot::yLeft, 0.0, 8.0);

    for (int axis = 0; axis < QwtPlot::axisCnt; ++axis) {
        if (auto *scaleWidget = ui->FFT_plot->axisWidget(axis)) {
            QPalette pal = scaleWidget->palette();
            pal.setColor(QPalette::WindowText, lightGray);
            pal.setColor(QPalette::Text, lightGray);
            scaleWidget->setPalette(pal);
        }
    }

    QwtPlotGrid *timeGrid = new QwtPlotGrid();
    timeGrid->setMajorPen(QColor(183, 182, 191, 80), 0.5);
    timeGrid->enableX(true);
    timeGrid->enableY(true);
    timeGrid->attach(ui->Time_plot);

    ui->Time_plot->setAxisMaxMajor(QwtPlot::yLeft, 4);
    ui->Time_plot->setCanvasBackground(backgroundColor);
    QwtText timeTitle("Time Domain");
    timeTitle.setColor(white);
    ui->Time_plot->setTitle(timeTitle);
    QwtText timeX("Time (\u00b5s)");
    timeX.setColor(lightGray);
    ui->Time_plot->setAxisTitle(QwtPlot::xBottom, timeX);
    QwtText timeY("Signal Value");
    timeY.setColor(lightGray);
    ui->Time_plot->setAxisTitle(QwtPlot::yLeft, timeY);
    ui->Time_plot->setAxisScale(QwtPlot::xBottom, 0, timeWindowSeconds * 1e6);

    for (int axis = 0; axis < QwtPlot::axisCnt; ++axis) {
        if (auto *scaleWidget = ui->Time_plot->axisWidget(axis)) {
            QPalette pal = scaleWidget->palette();
            pal.setColor(QPalette::WindowText, lightGray);
            pal.setColor(QPalette::Text, lightGray);
            scaleWidget->setPalette(pal);
        }
    }

    this->centralWidget()->setStyleSheet("background-color: rgb(13, 13, 13);");

    QTimer *plotTimer = new QTimer(this);
    connect(plotTimer, &QTimer::timeout, this, &MainWindow::updatePlot);
    plotTimer->start(plotRefreshRateMs);

    int requiredSize = computeRequiredSampleCount();
    set_time_buffer_size(requiredSize);

    register_peak_callback([](double freq) {
        QMetaObject::invokeMethod(qApp, [freq]() {
            auto windows = qApp->topLevelWidgets();
            for (auto *w : windows) {
                if (auto *mw = qobject_cast<MainWindow *>(w)) {
                    mw->updatePeakFrequency(freq);
                    break;
                }
            }
        });
    });

    QTimer::singleShot(0, this, []() {
        QThread* fftThread = QThread::create([]() { start_fft_stream(); });
        fftThread->start();

        QThread* timeThread = QThread::create([]() { start_time_stream(); });
        timeThread->start();
    });
}

MainWindow::~MainWindow() { delete ui; } // destructor

int MainWindow::computeRequiredSampleCount() const {  // required function
    return std::min(static_cast<int>(sampleRate * timeWindowSeconds + 1), MAX_BUFFER_LIMIT); // +1 fencepost
}

// mode change function from dropdown
void MainWindow::onModeChanged(int index) {
    if (index == 0)
        currentMode = FFTMode::FullBandwidth;
    else
        currentMode = FFTMode::LowBandwidth;

    qDebug() << "Mode changed to:" << ((currentMode == FFTMode::FullBandwidth) ? "Full" : "Low");
    restartStreamsForMode();
}

void MainWindow::restartStreamsForMode() {
    set_fft_mode(static_cast<int>(currentMode));

    sampleRate = (currentMode == FFTMode::FullBandwidth) ? 80e6 : 200000;
    int requiredSize = computeRequiredSampleCount();
    set_time_buffer_size(requiredSize);

    // Register peak callback on main thread (safe)
    register_peak_callback([](double freq) {
        QMetaObject::invokeMethod(qApp, [freq]() {
            auto windows = qApp->topLevelWidgets();
            for (auto *w : windows) {
                if (auto *mw = qobject_cast<MainWindow *>(w)) {
                    mw->updatePeakFrequency(freq);
                    break;
                }
            }
        });
    });

    // Launch threads (non-blocking)
    QThread* fftThread  = QThread::create([]() { start_fft_stream(); });
    QThread* timeThread = QThread::create([]() { start_time_stream(); });
    fftThread->start();
    timeThread->start();

    qDebug() << "Streams restarted for mode:" << ((currentMode == FFTMode::FullBandwidth) ? "FullBandwidth" : "LowBandwidth");
}

void MainWindow::updatePlot() {
    if (isPaused) return; // pause toggle

    double fftBuffer[FFT_BINS] = {0};
    get_fft_magnitudes(fftBuffer, FFT_BINS);

    // Avoid blinking FFT plot by checking if data looks valid
    if (fftBuffer[10] > 0.0)
        plotManager->updateFFT(fftBuffer, sampleRate);

    int targetSamples   = computeRequiredSampleCount(); // Plus 1 (fence post adjustment, ensures enough points
    int availableSamples = get_time_sample_count();
    int plotSamples     = std::min(targetSamples, availableSamples);

    if (plotSamples > 0) {
        std::vector<uint16_t> buffer(plotSamples);
        get_time_domain_buffer(buffer.data(), plotSamples);
        plotManager->updateTime(buffer, sampleRate, timeWindowSeconds, maxPointsToPlot);
    }
}

void MainWindow::togglePause() { isPaused = !isPaused; }

void MainWindow::promptUserToSavePlot() {
    QStringList options;
    options << "Save Time-Domain Plot" << "Save Frequency (FFT) Plot";

    bool ok;
    QString choice = QInputDialog::getItem(this, "select plot", "Choose which Plot data to export:",
                                           options, 0, false, &ok);
    if (!ok || choice.isEmpty()) return;

    if (choice == "Save Time-Domain Plot")
        saveTimePlotToFile();
    else if (choice == "Save Frequency (FFT) Plot")
        saveFFTPlotToFile();
}

// Time‑domain export
void MainWindow::saveTimePlotToFile() {
    QSettings settings("Ultracoustics", "RealtimePlotApp");
    QString lastDir = settings.value("lastSavePath", QDir::homePath()).toString();

    QString fileName = QFileDialog::getSaveFileName(this, "Save Time-Domain Plot Data",
                                                    lastDir,
                                                    "Text File (*.txt);;CSV File (*.csv);;Raw Binary (*.raw)");
    if (fileName.isEmpty()) return;
    if (fileName.endsWith(".raw")) return;  // ignore raw format for now

    settings.setValue("lastSavePath", QFileInfo(fileName).absolutePath());

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << fileName;
        return;
    }

    QTextStream out(&file);
    double timeStep_us = 1e6 / sampleRate;
    int exportSize = std::min(static_cast<int>(sampleRate * timeWindowSeconds), MAX_BUFFER_LIMIT);
    std::vector<uint16_t> timePlotBuffer(exportSize);
    get_time_domain_buffer(timePlotBuffer.data(), exportSize);

    out << "Time (us),Power (uW)\n";
    for (int i = 0; i < exportSize; ++i) {
        double time_us = i * timeStep_us;
        double power_uW = (static_cast<double>(timePlotBuffer[i]) - 49555.0) * 0.0147;
        out << time_us << "," << power_uW << "\n";
    }
    file.close();
}

// FFT export
void MainWindow::saveFFTPlotToFile() {
    QSettings settings("Ultracoustics", "RealtimePlotApp");
    QString lastDir = settings.value("lastSavePath", QDir::homePath()).toString();

    QString fileName = QFileDialog::getSaveFileName(this, "Save Frequency-Domain Plot Data",
                                                    lastDir,
                                                    "Text File (*.txt);;CSV File (*.csv);;Raw Binary (*.raw)");
    if (fileName.isEmpty()) return;
    if (fileName.endsWith(".raw")) return;

    settings.setValue("lastSavePath", QFileInfo(fileName).absolutePath());

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << fileName;
        return;
    }
    QTextStream out(&file);
    if (sampleRate > 1e6) {
        out << "Frequency (MHz),Log Magnitude\n";
    } else {
        out << "Frequency (KHz),Log Magnitude\n";
    }

    double fftBuffer[FFT_BINS] = {0};
    get_fft_magnitudes(fftBuffer, FFT_BINS);

    for (int i = 0; i < FFT_BINS; ++i) {
        double freq = 0;
        if (sampleRate > 1e6) {
            freq = i * (sampleRate / FFT_SIZE) / 1e6;
        } else {
            freq = i * (sampleRate / FFT_SIZE) / 1e3;
        }
        double mag = std::max(fftBuffer[i], EPSILON);
        out << freq << "," << log10(mag) << "\n";
    }
    file.close();
}

void MainWindow::updatePeakFrequency(double frequency) {
    QString unit = (currentMode == FFTMode::LowBandwidth) ? "kHz" : "MHz";
    QString label = QString("Peak: %1 %2").arg(frequency, 0, 'f', 2).arg(unit);
    ui->PeakFreq->setText(label);
}


