#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "AppConfig.h"
#include "FFTProcess.h"
#include "TimeDProcess.h"
#include "PlotManager.h"
#include "Features.h"
#include "MenuBarStyler.h"

#include <QTimer>
#include <QDebug>
#include <QLayout>
#include <QKeyEvent>
#include <QMouseEvent>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), fft(new FFTProcess()), time(new TimeDProcess()), plotManager(nullptr)
{
    ui->setupUi(this);
    MenuBarStyler::applyDarkTheme(ui->menuBar);

    // Remove layout margins so zoom buttons align with axes
    if (ui->mainLayout)
        ui->mainLayout->setContentsMargins(0, 0, 0, 20);  // 4th - bottom spacing
    if (ui->sideLayout)
        ui->sideLayout->setContentsMargins(0, 0, 0, 0);
    if (ui->gridLayoutFFT)
        ui->gridLayoutFFT->setContentsMargins(0, 0, 0, 20); // 4th - freq / splitter spacing
    if (ui->gridLayoutTime)
        ui->gridLayoutTime->setContentsMargins(0, 20, 0, 0); //2nd - time / splitter spacing
    if (ui->horizontalSplitter) {
        ui->horizontalSplitter->setStretchFactor(0, 1);
        ui->horizontalSplitter->setStretchFactor(1, 0);
        ui->horizontalSplitter->setMouseTracking(true);
        ui->horizontalSplitter->installEventFilter(this);
    }

    if (ui->sidePanel) {
        ui->sidePanel->hide();
        ui->sidePanel->setMouseTracking(true);
        ui->sidePanel->installEventFilter(this);
    }

    QColor lightGray(183, 182, 191);
    QColor backgroundColor(13, 13, 13);
    QColor white(255, 255, 255);

    // Icons and button sizes
    ui->PausePlay->setIcon(QIcon(":/pause-svg.svg"));
    ui->PausePlay->setIconSize(QSize(48, 48));
    ui->Save->setIcon(QIcon(":/download-svg.svg"));
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
    ui->splitter->setHandleWidth(1);
    ui->splitter->setStyleSheet("QSplitter::handle { background-color: rgb(100, 100, 100); }");  // EDIT


    // General background color for app
    this->centralWidget()->setStyleSheet("background-color: rgb(13, 13, 13);");

    // Force window to front
    this->setGeometry(100, 100, 800, 600);
    this->raise();
    this->activateWindow();

    // Proper single initialization of plot manager
    plotManager = new PlotManager(ui->FFT_plot, ui->Time_plot, this);
    plotManager->resetFFTView();

    connect(ui->PausePlay, &QPushButton::clicked, this, [=]() {
        Features::togglePause(isPaused);
        if (isPaused)
            ui->PausePlay->setIcon(QIcon(":/play-svg.svg"));
        else
            ui->PausePlay->setIcon(QIcon(":/pause-svg.svg"));
    });

    connect(ui->modes, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int) {
        Features::switchMode(currentMode);
        time->resize(static_cast<int>(AppConfig::sampleRate * AppConfig::timeWindowSeconds + 1));
        fft->setMode(currentMode);
        plotManager->resetFFTView();

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

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F11) {
        toggleFullScreen();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::toggleFullScreen()
{
    if (isFullScreen) {
        ui->menuBar->show();
        showNormal();
        isFullScreen = false;
    } else {
        ui->menuBar->hide();
        showFullScreen();
        isFullScreen = true;
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->horizontalSplitter && event->type() == QEvent::MouseMove) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        int threshold = 20;
        int x = me->pos().x();
        int width = ui->horizontalSplitter->width();
        int panelLeft = width - ui->sidePanel->width();

        if (!ui->sidePanel->isVisible() && x > width - threshold) {
            ui->sidePanel->show();
        } else if (ui->sidePanel->isVisible() && x < panelLeft && !ui->sidePanel->underMouse()) {
            ui->sidePanel->hide();
        }
    } else if (obj == ui->sidePanel) {
        if (event->type() == QEvent::Leave) {
            ui->sidePanel->hide();
        } else if (event->type() == QEvent::Enter) {
            ui->sidePanel->show();
        }
    }
    return QMainWindow::eventFilter(obj, event);
}
