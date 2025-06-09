#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "AppConfig.h"
#include "FFTProcess.h"
#include "TimeDProcess.h"
#include "PlotManager.h"
#include "Features.h"

#include <QTimer>
#include <QDebug>
#include <QLayout>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), fft(new FFTProcess()), time(new TimeDProcess()), plotManager(nullptr)
{
    ui->setupUi(this);

    // Remove layout margins so zoom buttons align with axes
    if (ui->mainLayout)
        ui->mainLayout->setContentsMargins(0, 0, 0, 0);
    if (ui->sideLayout)
        ui->sideLayout->setContentsMargins(0, 0, 0, 0);
    if (ui->gridLayoutFFT)
        ui->gridLayoutFFT->setContentsMargins(0, 0, 0, 0);
    if (ui->gridLayoutTime)
        ui->gridLayoutTime->setContentsMargins(0, 0, 0, 0);
    if (ui->horizontalSplitter) {
        ui->horizontalSplitter->setStretchFactor(0, 1);
        ui->horizontalSplitter->setStretchFactor(1, 0);
    }

    QColor lightGray(183, 182, 191);
    QColor backgroundColor(13, 13, 13);
    QColor white(255, 255, 255);

    // Icons and button sizes
    ui->PausePlay->setIcon(QIcon(":/play-pause-icon.png"));
    ui->PausePlay->setIconSize(QSize(48, 48));
    ui->Save->setIcon(QIcon(":/download-icon.png"));
    ui->Save->setIconSize(QSize(48, 48));

    // Peak Frequency label
    ui->PeakFreq->setText("Peak: --");
    ui->PeakFreq->setStyleSheet("color: gray; font-size: 18px;");
    ui->PeakFreq->setAlignment(Qt::AlignCenter);
    ui->PeakFreq->setMinimumWidth(80);  // or any width that fits max expected text


    // ComboBox (mode selector) styling
    ui->modes->setStyleSheet(
        "QComboBox { color: white; background-color: rgb(95, 95, 95); border: 1px solid gray; }"
        "QComboBox QAbstractItemView { background-color: rgb(95, 95, 95); color: white; }"
        );

    // Splitter setup
    QList<int> initialSizes { height() / 2, height() / 2 };
    ui->splitter->setSizes(initialSizes);
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setChildrenCollapsible(true);

    // General background color for app
    this->centralWidget()->setStyleSheet("background-color: rgb(13, 13, 13);");

    // Force window to front
    this->setGeometry(100, 100, 800, 600);
    this->raise();
    this->activateWindow();

    // Proper single initialization of plot manager
    plotManager = new PlotManager(ui->FFT_plot, ui->Time_plot, this);

    connect(ui->PausePlay, &QPushButton::clicked, this, [=]() {
        Features::togglePause(isPaused);
    });

    connect(ui->modes, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int) {
        Features::switchMode(currentMode);
        time->resize(static_cast<int>(AppConfig::sampleRate * AppConfig::timeWindowSeconds + 1));
        fft->setMode(currentMode);

        qDebug() << "[MainWindow] Mode change: Starting FFT...";
        fft->start();
        qDebug() << "[MainWindow] FFT started.";

        qDebug() << "[MainWindow] Starting Time...";
        time->start();
        qDebug() << "[MainWindow] Time started.";
    });

    connect(ui->Save, &QPushButton::clicked, this, [=]() {
        std::vector<double> fftBuf(AppConfig::fftBins, 0.0);
        fft->getMagnitudes(fftBuf.data(), AppConfig::fftBins);

        int count = time->sampleCount();
        std::vector<uint16_t> timeBuf(count);
        time->getBuffer(timeBuf.data(), count);

        Features::promptUserToSavePlot(this, fftBuf.data(), timeBuf);
    });

    connect(fft, &FFTProcess::peakFrequencyUpdated, this, [=](double freq) {
        Features::updatePeakFrequency(ui->PeakFreq, currentMode, freq, isPaused);
    });

    QTimer *plotTimer = new QTimer(this);
    connect(plotTimer, &QTimer::timeout, this, [=]() {
        plotManager->updatePlot(fft, time, isPaused, currentMode);
    });
    plotTimer->start(AppConfig::plotRefreshRateMs);

    fft->setMode(currentMode);

    // Delay starting the streaming until after the GUI is fully shown
     QTimer::singleShot(0, this, [this] {
        qDebug() << "[MainWindow] Delayed FFT and Time start...";

        fft->start();
        qDebug() << "[MainWindow] FFT started.";

        time->start();
        qDebug() << "[MainWindow] Time started.";
    });           // dont start yet
}

MainWindow::~MainWindow() {
    delete fft;     // safe since no parent
    delete time;     // safe since no parent
    delete plotManager;
    delete ui;
}
